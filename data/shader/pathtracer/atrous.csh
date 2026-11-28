#include <../common/utility.hsh>
#include <../common/convert.hsh>
#include <../common/flatten.hsh>
#include <../common/normalencode.hsh>

#include <../common/denoiser.hsh>

layout (local_size_x = 16, local_size_y = 16) in;

layout(set = 3, binding = 0, rgba16f) writeonly uniform image2D outputImage;
layout(set = 3, binding = 1) uniform sampler2D inputTexture;
layout(set = 3, binding = 2) uniform sampler2D depthTexture;
layout(set = 3, binding = 3) uniform sampler2D normalTexture;
layout(set = 3, binding = 4) uniform usampler2D materialIdxTexture;
#ifdef REMODULATE
layout(set = 3, binding = 5) uniform sampler2D albedoTexture;
layout(set = 3, binding = 6) uniform sampler2D emissionTexture;
#endif

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
    // Contains 16 bit color, variance and normal
    uvec4 data;
};

// Separate arrays, a struct with them would be padded to 32 bytes and exceed 32 KB of shared memory on some devices
shared PackedPixelData pixelData[sharedDataSize];
shared uint pixelMaterialIdx[sharedDataSize];
shared float pixelDepth[sharedDataSize];

layout(push_constant) uniform constants {
    float strength;
} pushConstants;

const float kernelWeights[3] = { 1.0, 2.0 / 3.0, 1.0 / 6.0 };

PackedPixelData PackPixelData(PixelData data) {
    PackedPixelData compressed;

    compressed.data.x = packHalf2x16(data.color.rg);
    compressed.data.y = packHalf2x16(data.color.ba);
    compressed.data.z = packHalf2x16(data.normal.xy);
    compressed.data.w = packHalf2x16(vec2(data.normal.z, 0.0));

    return compressed;
}

PixelData UnpackPixelData(PackedPixelData compressed) {
    PixelData data;

    data.color.rg = unpackHalf2x16(compressed.data.x);
    data.color.ba = unpackHalf2x16(compressed.data.y);
    data.normal.xy = unpackHalf2x16(compressed.data.z);
    vec2 temp = unpackHalf2x16(compressed.data.w);
    data.normal.z = temp.x;

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
        data.depth = depth >= 1.0 ? 0.0 : ConvertDepthToViewSpaceDepth(depth);

        data.materialIdx = texelFetch(materialIdxTexture, texel, 0).r;

        // The path tracer writes world space normals
        data.normal = DecodeNormal(texelFetch(normalTexture, texel, 0).rg);

        pixelData[i] = PackPixelData(data);
        pixelMaterialIdx[i] = data.materialIdx;
        pixelDepth[i] = data.depth;
    }

    barrier();

}

PixelData GetPixel(ivec2 pixelOffset) {

    ivec2 pixel = ivec2(gl_LocalInvocationID) + ivec2(kernelRadius);
    int sharedMemoryIdx = Flatten2D(pixel + pixelOffset, unflattenedSharedDataSize);
    PixelData data = UnpackPixelData(pixelData[sharedMemoryIdx]);
    data.materialIdx = pixelMaterialIdx[sharedMemoryIdx];
    data.depth = pixelDepth[sharedMemoryIdx];
    return data;

}

float GetFilteredVariance() {

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

vec4 Filter(ivec2 pixel, ivec2 resolution, PixelData centerPixelData) {

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(resolution);
    vec2 texelSize = 1.0 / vec2(resolution);

    vec4 centerColor = centerPixelData.color;
    vec3 centerNormal = centerPixelData.normal;
    uint centerMaterialIdx = centerPixelData.materialIdx;

    vec3 viewRay = GetViewRay(texCoord);
    vec3 viewRayDx = GetViewRay(texCoord + vec2(texelSize.x, 0.0)) - viewRay;
    vec3 viewRayDy = GetViewRay(texCoord + vec2(0.0, texelSize.y)) - viewRay;
    vec3 centerPosition = viewRay * centerPixelData.depth;
    vec3 centerViewNormal = normalize(mat3(globalData.vMatrix) * centerNormal);

    float centerLuminance = Luma(centerColor.rgb);

    vec4 outputColor = centerColor;
    float totalWeight = 1.0;

    float variance = GetFilteredVariance();
    float stdDeviation = sqrt(max(0.0, variance));

    vec3 viewDir = normalize(centerPosition);
    float NdotV = abs(dot(viewDir, centerViewNormal));
    float depthPhi = max(32.0, NdotV * 128.0);

    // All weights of a tap go into one exponent, the terms that don't depend on the tap are computed once.
    // The distance to the surface plane is affine in the offset since the view ray is
    const float log2e = 1.442695;
    float planeRay = dot(viewRay, centerViewNormal);
    float planeRayDx = dot(viewRayDx, centerViewNormal);
    float planeRayDy = dot(viewRayDy, centerViewNormal);
    float planeOffset = dot(centerPosition, centerViewNormal);
    float planeScale = depthPhi * log2e / abs(centerPosition.z);
    float luminanceScale = log2e / (stdDeviation * pushConstants.strength + 1e-10);
    const float normalPhi = 128.0;

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
                abs(planeDistance) * planeScale - normalPhi * log2(normalDot);
            float weight = kernelWeights[abs(x)] * kernelWeights[abs(y)] * exp2(-exponent);

            totalWeight += weight;
            outputColor += vec4(vec3(weight), weight * weight) * sampleColor;
        }
    }

    return outputColor / vec4(vec3(totalWeight), totalWeight * totalWeight);

}

void main() {

    LoadGroupSharedData();

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    ivec2 resolution = imageSize(outputImage);
    if (pixel.x >= resolution.x || pixel.y >= resolution.y)
        return;

    PixelData centerPixelData = GetPixel(ivec2(0));
    vec4 outputColor = centerPixelData.depth == 0.0 ? texelFetch(inputTexture, pixel, 0) :
        Filter(pixel, resolution, centerPixelData);

#ifdef REMODULATE
    vec3 albedo = texelFetch(albedoTexture, pixel, 0).rgb;
    vec3 emission = texelFetch(emissionTexture, pixel, 0).rgb;
    outputColor = vec4(min(outputColor.rgb * albedo + emission, vec3(65000.0)), 1.0);
#endif

    imageStore(outputImage, pixel, outputColor);

}
