#include <../common/utility.hsh>
#include <../common/convert.hsh>
#include <../common/PI.hsh>
#include <../common/stencil.hsh>
#include <../common/flatten.hsh>
#include <../common/random.hsh>
#include <../common/normalencode.hsh>

layout (local_size_x = 16, local_size_y = 16) in;

layout(set = 3, binding = 0, r16f) writeonly uniform image2D resolveImage;
layout(set = 3, binding = 1, r16f) writeonly uniform image2D historyLengthImage;

layout(set = 3, binding = 2) uniform sampler2D currentTexture;
layout(set = 3, binding = 3) uniform sampler2D velocityTexture;
layout(set = 3, binding = 4) uniform sampler2D depthTexture;
layout(set = 3, binding = 5) uniform sampler2D roughnessMetallicAoTexture;
layout(set = 3, binding = 6) uniform sampler2D normalTexture;
layout(set = 3, binding = 7) uniform usampler2D materialIdxTexture;

layout(set = 3, binding = 8) uniform sampler2D historyTexture;
layout(set = 3, binding = 9) uniform sampler2D historyLengthTexture;
layout(set = 3, binding = 10) uniform sampler2D historyDepthTexture;
layout(set = 3, binding = 11) uniform sampler2D historyNormalTexture;
layout(set = 3, binding = 12) uniform usampler2D historyMaterialIdxTexture;

layout(push_constant) uniform constants {
    int resetHistory;
} pushConstants;

vec2 invResolution = 1.0 / vec2(imageSize(resolveImage));
vec2 resolution = vec2(imageSize(resolveImage));

const int kernelRadius = 4;

const uint sharedDataSize = (gl_WorkGroupSize.x + 2 * kernelRadius) * (gl_WorkGroupSize.y + 2 * kernelRadius);
const ivec2 unflattenedSharedDataSize = ivec2(gl_WorkGroupSize) + 2 * kernelRadius;

shared vec2 sharedAoDepth[sharedDataSize];

const ivec2 offsets[9] = ivec2[9](
    ivec2(-1, -1),
    ivec2(0, -1),
    ivec2(1, -1),
    ivec2(-1, 0),
    ivec2(0, 0),
    ivec2(1, 0),
    ivec2(-1, 1),
    ivec2(0, 1),
    ivec2(1, 1)
);

const ivec2 pixelOffsets[4] = ivec2[4](
    ivec2(0, 0),
    ivec2(1, 0),
    ivec2(0, 1),
    ivec2(1, 1)
);

float FetchTexel(ivec2 texel) {
    
    float value = max(texelFetch(currentTexture, texel, 0).r, 0);
    return value;

}

void LoadGroupSharedData() {

    ivec2 workGroupOffset = ivec2(gl_WorkGroupID) * ivec2(gl_WorkGroupSize) - ivec2(kernelRadius);

    uint workGroupSize = gl_WorkGroupSize.x * gl_WorkGroupSize.y;
    for(uint i = gl_LocalInvocationIndex; i < sharedDataSize; i += workGroupSize) {
        ivec2 localOffset = Unflatten2D(int(i), unflattenedSharedDataSize);
        ivec2 texel = localOffset + workGroupOffset;

        texel = clamp(texel, ivec2(0), ivec2(resolution) - ivec2(1));

        sharedAoDepth[i].r = FetchTexel(texel);
        sharedAoDepth[i].g = ConvertDepthToViewSpaceDepth(texelFetch(depthTexture, texel, 0).r);
    }

    barrier();

}

int GetSharedMemoryIndex(ivec2 pixelOffset) {

    ivec2 pixel = ivec2(gl_LocalInvocationID) + ivec2(kernelRadius);
    return Flatten2D(pixel + pixelOffset, unflattenedSharedDataSize);

}

float FetchCurrentAo(int sharedMemoryIdx) {

    return sharedAoDepth[sharedMemoryIdx].r;

}

float FetchDepth(int sharedMemoryIdx) {

    return sharedAoDepth[sharedMemoryIdx].g;

}
ivec2 FindNearest3x3(ivec2 pixel) {

    ivec2 offset = ivec2(0);
    float depth = 1.0;

    for (int i = 0; i < 9; i++) {
        ivec2 offsetPixel = clamp(pixel + offsets[i], ivec2(0), ivec2(resolution) - ivec2(1));

        float currDepth = texelFetch(depthTexture, offsetPixel, 0).r;
        if (currDepth < depth) {
            depth = currDepth;
            offset = offsets[i];
        }
    }

    return offset;

}

void ComputeVarianceMinMax(out float mean, out float std) {

    float m1 = 0.0;
    float m2 = 0.0;
    // This could be varied using the temporal variance estimation
    // By using a wide neighborhood for variance estimation (8x8) we introduce block artifacts
    // These are similiar to video compression artifacts, the spatial filter mostly clears them up
    const int radius = kernelRadius;
    ivec2 pixel = ivec2(gl_GlobalInvocationID);

    uint materialIdx = texelFetch(materialIdxTexture, pixel, 0).r;

    float depth = texelFetch(depthTexture, pixel, 0).r;
    float linearDepth = ConvertDepthToViewSpaceDepth(depth);

    float totalWeight = 0.0;

    for (int i = -radius; i <= radius; i++) {
        for (int j = -radius; j <= radius; j++) {
            int sharedMemoryIdx = GetSharedMemoryIndex(ivec2(i, j));

            float sampleAo = FetchCurrentAo(sharedMemoryIdx);
            float sampleLinearDepth = FetchDepth(sharedMemoryIdx);

            float depthPhi = max(1.0, abs(0.025 * linearDepth));
            float weight = min(1.0 , exp(-abs(linearDepth - sampleLinearDepth)));
        
            m1 += sampleAo * weight;
            m2 += sampleAo * sampleAo * weight;

            totalWeight += weight;
        }
    }

    mean = m1 / totalWeight;
    std = sqrt(max((m2 / totalWeight) - (mean * mean), 0.0));

}

bool SampleHistory(ivec2 pixel, vec2 historyPixel, out float history, out float historyLength) {

    bool valid = true;
    
    history = 0.0;
    historyLength = 0.0;

    float totalWeight = 0.0;
    float x = fract(historyPixel.x);
    float y = fract(historyPixel.y);

    float weights[4] = { (1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y };

    vec3 normal = DecodeNormal(texelFetch(normalTexture, pixel, 0).rg);
    float depth = texelFetch(depthTexture, pixel, 0).r;

    float linearDepth = ConvertDepthToViewSpaceDepth(depth);
    float depthPhi = 16.0 / abs(linearDepth);

    // Calculate confidence over 2x2 bilinear neighborhood
    // Note that 3x3 neighborhoud could help on edges
    for (int i = 0; i < 4; i++) {
        ivec2 offsetPixel = ivec2(historyPixel) + pixelOffsets[i];
        float confidence = 1.0;

        offsetPixel = clamp(offsetPixel, ivec2(0), ivec2(resolution) - ivec2(1));

        vec3 historyNormal = DecodeNormal(texelFetch(historyNormalTexture, offsetPixel, 0).rg);
        confidence *= pow(dot(historyNormal, normal), 16.0);

        float historyDepth = texelFetch(historyDepthTexture, offsetPixel, 0).r;
        float historyLinearDepth = ConvertDepthToViewSpaceDepth(historyDepth);
        confidence *= min(1.0 , exp(-abs(linearDepth - historyLinearDepth) * depthPhi));

        if (confidence > 0.2) {
            totalWeight += weights[i];
            history += texelFetch(historyTexture, offsetPixel, 0).r * weights[i];
            historyLength += texelFetch(historyLengthTexture, offsetPixel, 0).r * weights[i];
        }
    }

    if (totalWeight > 0.0) {
        valid = true;
        history /= totalWeight;
        historyLength /= totalWeight;
    }
    else {
        valid = false;
        history = 0.0;
        historyLength = 0.0;
    }

    return valid;

}

void main() {

    LoadGroupSharedData();

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(resolveImage).x ||
        pixel.y > imageSize(resolveImage).y)
        return;

    float mean, std;
    ComputeVarianceMinMax(mean, std);

    const float historyClipFactor = .3;
    float historyNeighbourhoodMin = mean - historyClipFactor * std;
    float historyNeighbourhoodMax = mean + historyClipFactor * std;

    float currentNeighbourhoodMin = mean - std;
    float currentNeighbourhoodMax = mean + std;

    ivec2 velocityPixel = pixel;
    vec2 velocity = texelFetch(velocityTexture, velocityPixel, 0).rg;

    vec2 uv = (vec2(pixel) + vec2(0.5)) * invResolution + velocity;
    vec2 historyPixel = vec2(pixel) + velocity * resolution;

    bool valid = true;
    float history;
    float historyLength;
    valid = SampleHistory(pixel, historyPixel, history, historyLength);

    float historyValue = history;
    float currentValue = texelFetch(currentTexture, pixel, 0).r;

    // In case of clipping we might also reject the sample. TODO: Investigate
    currentValue = clamp(currentValue, currentNeighbourhoodMin, currentNeighbourhoodMax);
    historyValue = clamp(historyValue, historyNeighbourhoodMin, historyNeighbourhoodMax);

    float factor = 0.95;
    factor = (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0
         || uv.y > 1.0) ? 0.0 : factor;

    factor = pushConstants.resetHistory > 0 ? 0.0 : factor;

    if (factor == 0.0 || !valid) {
        historyLength = 0.0;
    }

    factor = min(factor, historyLength / (historyLength + 1.0));
    float resolve = factor <= 0.0 ?  currentValue : mix(currentValue, historyValue, factor);

    imageStore(historyLengthImage, pixel, vec4(historyLength + 1.0, 0.0, 0.0, 0.0));
    imageStore(resolveImage, pixel, vec4(resolve, 0.0, 0.0, 0.0));

}