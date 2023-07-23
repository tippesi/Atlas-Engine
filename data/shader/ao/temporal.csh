#include <../common/utility.hsh>
#include <../common/convert.hsh>
#include <../common/PI.hsh>
#include <../common/stencil.hsh>
#include <../common/flatten.hsh>
#include <../common/random.hsh>

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
        float currDepth = texelFetch(depthTexture, pixel + offsets[i], 0).r;
        if (currDepth < depth) {
            depth = currDepth;
            offset = offsets[i];
        }
    }

    return offset;

}

void ComputeVarianceMinMax(out float aabbMin, out float aabbMax) {

    float m1 = 0.0;
    float m2 = 0.0;
    // This could be varied using the temporal variance estimation
    // By using a wide neighborhood for variance estimation (8x8) we introduce block artifacts
    // These are similiar to video compression artifacts, the spatial filter mostly clears them up
    const int radius = 4;
    ivec2 pixel = ivec2(gl_GlobalInvocationID);

    uint materialIdx = texelFetch(materialIdxTexture, pixel, 0).r;

    float depth = texelFetch(depthTexture, pixel, 0).r;
    float linearDepth = ConvertDepthToViewSpaceDepth(depth);

    for (int i = -radius; i <= radius; i++) {
        for (int j = -radius; j <= radius; j++) {
            int sharedMemoryIdx = GetSharedMemoryIndex(ivec2(i, j));

            float sampleAo = FetchCurrentAo(sharedMemoryIdx);
            float sampleLinearDepth = FetchDepth(sharedMemoryIdx);

            float depthPhi = max(1.0, abs(0.025 * linearDepth));
            float weight = min(1.0 , exp(-abs(linearDepth - sampleLinearDepth) / depthPhi));

            sampleAo *= weight;
        
            m1 += sampleAo;
            m2 += sampleAo * sampleAo;
        }
    }

    float oneDividedBySampleCount = 1.0 / float((2.0 * radius + 1.0) * (2.0 * radius + 1.0));
    float gamma = 1.0;
    float mu = m1 * oneDividedBySampleCount;
    float sigma = sqrt(max((m2 * oneDividedBySampleCount) - (mu * mu), 0.0));
    aabbMin = mu - gamma * sigma;
    aabbMax = mu + gamma * sigma;

}

bool SampleHistory(ivec2 pixel, vec2 historyPixel, out float history, out float historyLength) {

    bool valid = true;
    
    history = 0.0;
    historyLength = 0.0;

    float totalWeight = 0.0;
    float x = fract(historyPixel.x);
    float y = fract(historyPixel.y);

    float weights[4] = { (1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y };

    uint materialIdx = texelFetch(materialIdxTexture, pixel, 0).r;
    vec3 normal = 2.0 * texelFetch(normalTexture, pixel, 0).rgb - 1.0;
    float depth = texelFetch(depthTexture, pixel, 0).r;

    float linearDepth = ConvertDepthToViewSpaceDepth(depth);

    // Calculate confidence over 2x2 bilinear neighborhood
    // Note that 3x3 neighborhoud could help on edges
    for (int i = 0; i < 4; i++) {
        ivec2 offsetPixel = ivec2(historyPixel) + pixelOffsets[i];
        float confidence = 1.0;

        uint historyMaterialIdx = texelFetch(historyMaterialIdxTexture, offsetPixel, 0).r;
        confidence *= historyMaterialIdx != materialIdx ? 0.0 : 1.0;

        vec3 historyNormal = 2.0 * texelFetch(historyNormalTexture, offsetPixel, 0).rgb - 1.0;
        confidence *= pow(abs(dot(historyNormal, normal)), 2.0);

        float historyDepth = texelFetch(historyDepthTexture, offsetPixel, 0).r;
        float historyLinearDepth = ConvertDepthToViewSpaceDepth(historyDepth);
        confidence *= min(1.0 , exp(-abs(linearDepth - historyLinearDepth)));

        if (confidence > 0.1) {
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

    float localNeighbourhoodMin, localNeighbourhoodMax;
    ComputeVarianceMinMax(localNeighbourhoodMin, localNeighbourhoodMax);

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
    currentValue = clamp(currentValue, localNeighbourhoodMin, localNeighbourhoodMax);
    historyValue = clamp(historyValue, localNeighbourhoodMin, localNeighbourhoodMax);

    float factor = 0.95;
    factor = (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0
         || uv.y > 1.0) ? 0.0 : factor;

    if (factor == 0.0 || !valid) {
        historyLength = 0.0;
    }

    factor = min(factor, historyLength / (historyLength + 1.0));

    float resolve = mix(currentValue, historyValue, factor);

    imageStore(historyLengthImage, pixel, vec4(historyLength + 1.0, 0.0, 0.0, 0.0));
    imageStore(resolveImage, pixel, vec4(resolve, 0.0, 0.0, 0.0));

}