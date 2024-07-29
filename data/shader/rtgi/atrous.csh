#include <../common/utility.hsh>
#include <../common/convert.hsh>
#include <../common/flatten.hsh>
#include <../common/barrier.hsh>
#include <../common/material.hsh>
#include <../common/normalencode.hsh>

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

layout(push_constant) uniform constants {
    int stepSize;
    float strength;
} pushConstants;

const float kernelWeights[3] = { 1.0, 2.0 / 3.0, 1.0 / 6.0 };

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

        data.depth = texelFetch(depthTexture, texel, 0).r;

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

float ComputeEdgeStoppingWeight(float centerLuminance, float sampleLuminance,
                                vec3 centerNormal, vec3 sampleNormal,
                                float centerDepth, float sampleDepth,
                                uint centerMaterialIdx, uint sampleMaterialIdx,
                                float luminancePhi, float normalPhi, 
                                float depthPhi, bool noHistory) {

    const float epsilon = 1e-10;

    float luminanceDiff = abs(centerLuminance - sampleLuminance);
    float luminanceWeight = min(exp(-luminanceDiff / (luminancePhi + epsilon)), 1.0);

    luminanceWeight = noHistory ? 1.0 : luminanceWeight;

    float normalDiff = saturate(dot(centerNormal, sampleNormal));
    float normalWeight = min(pow(normalDiff, normalPhi), 1.0);

    float depthDiff = abs(centerDepth - sampleDepth);
    float depthWeight = min(exp(-depthDiff * depthPhi), 1.0);

    float materialWeight = centerMaterialIdx == sampleMaterialIdx ? 1.0 : 0.0;

    return luminanceWeight * normalWeight * depthWeight * materialWeight;

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

    LoadGroupSharedData();

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    ivec2 resolution = imageSize(outputImage);

    if (pixel.x >= resolution.x || pixel.y >= resolution.y)
        return;

    PixelData centerPixelData = GetPixel(ivec2(0));
    
    vec4 centerColor = centerPixelData.color;
    float centerDepth = centerPixelData.depth;
    vec3 centerNormal = centerPixelData.normal;
    uint centerMaterialIdx = centerPixelData.materialIdx;

    if (centerDepth == 1.0)
        return;

    float historyLength = texelFetch(historyLengthTexture, pixel, 0).b;
    bool noHistory = historyLength <= 1.0;

    float centerLuminance = Luma(centerColor.rgb);
    float centerLinearDepth = centerDepth;
    
    vec4 outputColor = centerColor;
    float totalWeight = 1.0;

    float variance = GetFilteredVariance(pixel);
    float stdDeviation = sqrt(max(0.0, variance));

    float depthPhi = 32.0 / abs(centerLinearDepth);

    const int radius = 2;
    for (int x = -radius; x <= radius; x++) {
        for (int y = -radius; y <= radius; y++) {            
            ivec2 samplePixel = pixel + ivec2(x, y) * pushConstants.stepSize;

            if (samplePixel.x >= resolution.x || samplePixel.y >= resolution.y ||
                samplePixel.x < 0 || samplePixel.y < 0
                || (x == 0 && y == 0))
                continue;

            PixelData samplePixelData = GetPixel(ivec2(x, y) * pushConstants.stepSize);

            if (samplePixelData.depth == 1.0)
                continue;

            vec4 sampleColor = samplePixelData.color;
            vec3 sampleNormal = samplePixelData.normal;

            float sampleLinearDepth = samplePixelData.depth;
            float sampleLuminance = Luma(sampleColor.rgb);

            uint sampleMaterialIdx = samplePixelData.materialIdx;

            float kernelWeight = kernelWeights[abs(x)] * kernelWeights[abs(y)];
            float edgeStoppingWeight = ComputeEdgeStoppingWeight(
                                    centerLuminance, sampleLuminance,
                                    centerNormal, sampleNormal,
                                    centerLinearDepth, sampleLinearDepth,
                                    centerMaterialIdx, sampleMaterialIdx,
                                    stdDeviation * pushConstants.strength, 
                                    noHistory ? 2.0 : 32.0, 32.0, noHistory);

            float weight = kernelWeight * edgeStoppingWeight;
            
            totalWeight += weight;
            outputColor += vec4(vec3(weight), weight * weight) * sampleColor;
        }
    }

    outputColor = outputColor / vec4(vec3(totalWeight), totalWeight * totalWeight);

    imageStore(outputImage, pixel, outputColor);

}