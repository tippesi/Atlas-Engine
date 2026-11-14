#include <../common/utility.hsh>
#include <../common/convert.hsh>
#include <../common/flatten.hsh>
#include <../common/barrier.hsh>
#include <../common/material.hsh>
#include <../common/normalencode.hsh>

#include <../common/denoiser.hsh>

layout (local_size_x = 16, local_size_y = 16) in;

layout(set = 3, binding = 0, rgba16f) writeonly uniform image2D outputImage;
layout(set = 3, binding = 1) uniform sampler2D inputTexture;
layout(set = 3, binding = 2) uniform sampler2D depthTexture;
layout(set = 3, binding = 3) uniform sampler2D normalTexture;
layout(set = 3, binding = 4) uniform usampler2D materialIdxTexture;
layout(set = 3, binding = 5) uniform sampler2D historyLengthTexture;

#ifdef STEP_SIZE1
const int kernelRadius = 2;
#endif
#ifdef STEP_SIZE2
const int kernelRadius = 4;
#endif
#ifdef STEP_SIZE4
const int kernelRadius = 8;
#endif

const int stepSize = kernelRadius / 2;

const uint sharedDataSize = (gl_WorkGroupSize.x + 2 * kernelRadius) * (gl_WorkGroupSize.y + 2 * kernelRadius);
const ivec2 unflattenedSharedDataSize = ivec2(gl_WorkGroupSize) + 2 * kernelRadius;

struct PixelData {
    vec4 color;
    vec3 normal;
    uint materialIdx;
    float depth;
};

struct PackedPixelData {
    // Contains 16 bit color, variance, normal and roughness
    uvec4 data;
    uint materialIdx;
    float depth;
};

shared PackedPixelData pixelData[sharedDataSize];
shared uint groupFilters;

layout(push_constant) uniform constants {
    float strength;
} pushConstants;

const float kernelWeights[3] = { 1.0, 2.0 / 3.0, 1.0 / 6.0 };

// In pixels, the three passes together reach 14 pixels
const float maxBlurRadius = 16.0;
const float minBlurRadius = 1.5;

PackedPixelData PackPixelData(PixelData data) {
    PackedPixelData compressed;

    compressed.data.x = packHalf2x16(data.color.rg);
    compressed.data.y = packHalf2x16(data.color.ba);
    compressed.data.z = packHalf2x16(data.normal.xy);
    compressed.data.w = packHalf2x16(vec2(data.normal.z, 0.0));
    compressed.materialIdx = data.materialIdx;
    compressed.depth = data.depth;

    return compressed;
}

PixelData UnpackPixelData(PackedPixelData compressed) {
    PixelData data;

    data.color.rg = unpackHalf2x16(compressed.data.x);
    data.color.ba = unpackHalf2x16(compressed.data.y);
    data.normal.xy = unpackHalf2x16(compressed.data.z);
    vec2 temp = unpackHalf2x16(compressed.data.w);
    data.normal.z = temp.x;
    data.materialIdx = compressed.materialIdx;
    data.depth = compressed.depth;

    return data;
}

void LoadGroupSharedData() {

    ivec2 resolution = imageSize(outputImage);
    ivec2 workGroupOffset = ivec2(gl_WorkGroupID) * ivec2(gl_WorkGroupSize) - ivec2(kernelRadius);

    uint workGroupSize = gl_WorkGroupSize.x * gl_WorkGroupSize.y;
    for(uint i = gl_LocalInvocationIndex; i < sharedDataSize; i += workGroupSize) {
        ivec2 localOffset = Unflatten2D(int(i), unflattenedSharedDataSize);
        ivec2 texel = localOffset + workGroupOffset;

        texel = clamp(texel, ivec2(0), ivec2(resolution) - ivec2(1));

        PixelData data;

        data.color = texelFetch(inputTexture, texel, 0);

        // Linear depth, zero marks the sky
        float depth = texelFetch(depthTexture, texel, 0).r;
        data.depth = depth == 1.0 ? 0.0 : ConvertDepthToViewSpaceDepth(depth);

        data.materialIdx = texelFetch(materialIdxTexture, texel, 0).r;

        data.normal = DecodeNormal(texelFetch(normalTexture, texel, 0).rg);

        pixelData[i] = PackPixelData(data);
    }

    barrier();

}

PixelData GetPixel(ivec2 pixelOffset) {

    ivec2 pixel = ivec2(gl_LocalInvocationID) + ivec2(kernelRadius);
    int sharedMemoryIdx = Flatten2D(pixel + pixelOffset, unflattenedSharedDataSize);
    return UnpackPixelData(pixelData[sharedMemoryIdx]);

}

float Luma(vec3 color) {

    const vec3 luma = vec3(0.299, 0.587, 0.114);
    return dot(color, luma);

}

float GetFilteredVariance(ivec2 pixel) {

    float variance = 0.0;
    const float weights[2][2] =
    {
        { 1.0 / 4.0, 1.0 / 8.0  },
        { 1.0 / 8.0, 1.0 / 16.0 }
    };

    const int radius = 1;
    for (int x = -radius; x <= radius; x++) {
        for (int y = -radius; y <= radius; y++) {
            PixelData samplePixelData = GetPixel(ivec2(x, y));

            float weight = weights[abs(x)][abs(y)];
            float sampleVariance = samplePixelData.color.a;
            variance += weight * sampleVariance;
        }
    }

    return variance;

}

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    ivec2 resolution = imageSize(outputImage);
    bool insideImage = pixel.x < resolution.x && pixel.y < resolution.y;

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(resolution);
    vec2 texelSize = 1.0 / vec2(resolution);

    float depth = insideImage ? texelFetch(depthTexture, pixel, 0).r : 1.0;
    vec4 moments = insideImage ? texelFetch(historyLengthTexture, pixel, 0) : vec4(0.0);

    vec3 viewRay = GetViewRay(texCoord);
    vec3 centerPosition = viewRay * ConvertDepthToViewSpaceDepth(depth);

    float historyLength = moments.b;
    bool noHistory = historyLength <= 1.0;

    // The blur radius shrinks with the effectively accumulated frames and is limited by the hit distance.
    // Short histories need the blur more than the details, the limit only sets in while they grow
    float historyRadius = maxBlurRadius / sqrt(max(historyLength, 1.0));

    float hitDistance = DecodeHitDistance(moments.a);
    // The projection flips y, its scale is negative
    float pixelWorldSize = 2.0 * abs(centerPosition.z) / (abs(globalData.pMatrix[1][1]) * float(resolution.y));
    float hitRadius = max(hitDistance / pixelWorldSize, maxBlurRadius / max(historyLength, 1.0));

    float blurRadius = max(minBlurRadius, min(historyRadius, hitRadius));

    // Below half the step size the taps of this pass have no weight left
    bool filterPixel = insideImage && depth < 1.0 && blurRadius >= 0.5 * float(stepSize);

    if (gl_LocalInvocationIndex == 0)
        groupFilters = 0u;
    barrier();
    if (filterPixel)
        atomicOr(groupFilters, 1u);
    barrier();

    if (groupFilters != 0u)
        LoadGroupSharedData();

    if (!insideImage || depth == 1.0)
        return;

    if (!filterPixel) {
        imageStore(outputImage, pixel, texelFetch(inputTexture, pixel, 0));
        return;
    }

    vec3 viewRayDx = GetViewRay(texCoord + vec2(texelSize.x, 0.0)) - viewRay;
    vec3 viewRayDy = GetViewRay(texCoord + vec2(0.0, texelSize.y)) - viewRay;

    PixelData centerPixelData = GetPixel(ivec2(0));

    vec4 centerColor = centerPixelData.color;
    vec3 centerNormal = centerPixelData.normal;
    uint centerMaterialIdx = centerPixelData.materialIdx;

    float centerLuminance = Luma(centerColor.rgb);

    vec4 outputColor = centerColor;
    float totalWeight = 1.0;

    float variance = GetFilteredVariance(pixel);
    float stdDeviation = sqrt(max(0.0, variance));

    vec3 viewDir = normalize(centerPosition);
    float NdotV = abs(dot(viewDir, centerNormal));
    float depthPhi = max(32.0, NdotV * 128.0);

    // All weights of a tap go into one exponent, the terms that don't depend on the tap are computed once.
    // The distance to the surface plane is affine in the offset since the view ray is
    const float log2e = 1.442695;
    float planeRay = dot(viewRay, centerNormal);
    float planeRayDx = dot(viewRayDx, centerNormal);
    float planeRayDy = dot(viewRayDy, centerNormal);
    float planeOffset = dot(centerPosition, centerNormal);
    float planeScale = depthPhi * log2e / abs(centerPosition.z);
    float luminanceScale = noHistory ? 0.0 : log2e / (stdDeviation * pushConstants.strength + 1e-10);
    float normalPhi = noHistory ? 2.0 : NdotV * 128.0;
    float distanceScale = log2e * float(stepSize * stepSize) / sqr(blurRadius);

    const int radius = 2;
    for (int x = -radius; x <= radius; x++) {
        for (int y = -radius; y <= radius; y++) {
            ivec2 offset = ivec2(x, y) * stepSize;
            ivec2 samplePixel = pixel + offset;

            if (samplePixel.x >= resolution.x || samplePixel.y >= resolution.y ||
                samplePixel.x < 0 || samplePixel.y < 0
                || (x == 0 && y == 0))
                continue;

            PixelData samplePixelData = GetPixel(offset);

            if (samplePixelData.depth == 0.0 || samplePixelData.materialIdx != centerMaterialIdx)
                continue;

            vec4 sampleColor = samplePixelData.color;

            // The distance to the surface plane stays stable at grazing angles, unlike the depth
            float planeDistance = samplePixelData.depth * (planeRay + float(offset.x) * planeRayDx +
                float(offset.y) * planeRayDy) - planeOffset;
            float normalDot = max(dot(centerNormal, samplePixelData.normal), 1e-6);

            float exponent = abs(centerLuminance - Luma(sampleColor.rgb)) * luminanceScale +
                abs(planeDistance) * planeScale + float(x * x + y * y) * distanceScale -
                normalPhi * log2(normalDot);
            float weight = kernelWeights[abs(x)] * kernelWeights[abs(y)] * exp2(-exponent);
            
            totalWeight += weight;
            outputColor += vec4(vec3(weight), weight * weight) * sampleColor;
        }
    }

    outputColor = outputColor / vec4(vec3(totalWeight), totalWeight * totalWeight);
    
    imageStore(outputImage, pixel, outputColor);

}