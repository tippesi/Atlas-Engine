#include <../common/utility.hsh>
#include <../common/convert.hsh>
#include <../common/PI.hsh>
#include <../common/stencil.hsh>
#include <../common/flatten.hsh>
#include <../common/material.hsh>
#include <../common/random.hsh>
#include <../common/normalencode.hsh>

layout (local_size_x = 16, local_size_y = 16) in;

layout(set = 3, binding = 0, rgba16f) writeonly uniform image2D resolveImage;
layout(set = 3, binding = 1, rgba16f) writeonly uniform image2D momentsImage;

layout(set = 3, binding = 2) uniform sampler2D currentTexture;
layout(set = 3, binding = 3) uniform sampler2D velocityTexture;
layout(set = 3, binding = 4) uniform sampler2D depthTexture;
layout(set = 3, binding = 5) uniform sampler2D roughnessMetallicAoTexture;
layout(set = 3, binding = 6) uniform sampler2D normalTexture;
layout(set = 3, binding = 7) uniform usampler2D materialIdxTexture;

layout(set = 3, binding = 8) uniform sampler2D historyTexture;
layout(set = 3, binding = 9) uniform sampler2D historyMomentsTexture;
layout(set = 3, binding = 10) uniform sampler2D historyDepthTexture;
layout(set = 3, binding = 11) uniform sampler2D historyNormalTexture;
layout(set = 3, binding = 12) uniform usampler2D historyMaterialIdxTexture;

vec2 invResolution = 1.0 / vec2(imageSize(resolveImage));
vec2 resolution = vec2(imageSize(resolveImage));

const int kernelRadius = 5;

const uint sharedDataSize = (gl_WorkGroupSize.x + 2 * kernelRadius) * (gl_WorkGroupSize.y + 2 * kernelRadius);
const ivec2 unflattenedSharedDataSize = ivec2(gl_WorkGroupSize) + 2 * kernelRadius;

shared vec4 sharedRadianceDepth[sharedDataSize];

layout(push_constant) uniform constants {
    float temporalWeight;
    float historyClipMax;
    float currentClipFactor;
} pushConstants;

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

float Luma(vec3 color) {

    const vec3 luma = vec3(0.299, 0.587, 0.114);
    return dot(color, luma);

}

vec3 FetchTexel(ivec2 texel) {
    
    vec3 color = max(texelFetch(currentTexture, texel, 0).rgb, 0);
    return color;

}

void LoadGroupSharedData() {

    ivec2 workGroupOffset = ivec2(gl_WorkGroupID) * ivec2(gl_WorkGroupSize) - ivec2(kernelRadius);

    uint workGroupSize = gl_WorkGroupSize.x * gl_WorkGroupSize.y;
    for(uint i = gl_LocalInvocationIndex; i < sharedDataSize; i += workGroupSize) {
        ivec2 localOffset = Unflatten2D(int(i), unflattenedSharedDataSize);
        ivec2 texel = localOffset + workGroupOffset;

        texel = clamp(texel, ivec2(0), ivec2(resolution) - ivec2(1));

        sharedRadianceDepth[i].rgb = FetchTexel(texel);
        sharedRadianceDepth[i].a = ConvertDepthToViewSpaceDepth(texelFetch(depthTexture, texel, 0).r);
    }

    barrier();

}

int GetSharedMemoryIndex(ivec2 pixelOffset) {

    ivec2 pixel = ivec2(gl_LocalInvocationID) + ivec2(kernelRadius);
    return Flatten2D(pixel + pixelOffset, unflattenedSharedDataSize);

}

vec3 FetchCurrentRadiance(int sharedMemoryIdx) {

    return sharedRadianceDepth[sharedMemoryIdx].rgb;

}

float FetchDepth(int sharedMemoryIdx) {

    return sharedRadianceDepth[sharedMemoryIdx].a;

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

float ClipBoundingBox(vec3 boxMin, vec3 boxMax, vec3 history, vec3 current) {

    vec3 origin = history;
    vec3 dir = current - history;

    // Make sure dir isn't zero
    dir.x = abs(dir.x) < (1.0 / 32767.0) ? (1.0 / 32767.0) : dir.x;
    dir.y = abs(dir.y) < (1.0 / 32767.0) ? (1.0 / 32767.0) : dir.y;
    dir.z = abs(dir.z) < (1.0 / 32767.0) ? (1.0 / 32767.0) : dir.z;

    vec3 invDir = 1.0 / dir;

    vec3 t0 = (boxMin - origin) * invDir;
    vec3 t1 = (boxMax - origin) * invDir;

    vec3 intersect = min(t0, t1);
    return max(intersect.x, max(intersect.y, intersect.z));

}

bool SampleHistory(ivec2 pixel, vec2 historyPixel, out vec4 history, out vec4 historyMoments) {
    
    history = vec4(0.0);
    historyMoments = vec4(0.0);

    float totalWeight = 0.0;
    float x    = fract(historyPixel.x);
    float y    = fract(historyPixel.y);

    float weights[4] = { (1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y };

    uint materialIdx = texelFetch(materialIdxTexture, pixel, 0).r;
    vec3 normal = DecodeNormal(texelFetch(normalTexture, pixel, 0).rg);
    float depth = texelFetch(depthTexture, pixel, 0).r;

    float linearDepth = ConvertDepthToViewSpaceDepth(depth);

    // Calculate confidence over 2x2 bilinear neighborhood
    for (int i = 0; i < 4; i++) {
        ivec2 offsetPixel = ivec2(historyPixel) + pixelOffsets[i];
        float confidence = 1.0;

        uint historyMaterialIdx = texelFetch(historyMaterialIdxTexture, offsetPixel, 0).r;
        confidence *= historyMaterialIdx != materialIdx ? 0.0 : 1.0;

        vec3 historyNormal = DecodeNormal(texelFetch(historyNormalTexture, offsetPixel, 0).rg);
        confidence *= pow(abs(dot(historyNormal, normal)), 2.0);

        float historyDepth = texelFetch(historyDepthTexture, offsetPixel, 0).r;
        float historyLinearDepth = ConvertDepthToViewSpaceDepth(historyDepth);
        confidence *= min(1.0 , exp(-abs(linearDepth - historyLinearDepth)));

        if (confidence > 0.1) {
            totalWeight += weights[i];
            history += texelFetch(historyTexture, offsetPixel, 0) * weights[i];
            historyMoments += texelFetch(historyMomentsTexture, offsetPixel, 0) * weights[i];
        }
    }

    if (totalWeight > 0.0) {
        history /= totalWeight;
        historyMoments /= totalWeight;
        return true;
    }

    for (int i = 0; i < 9; i++) {
        ivec2 offsetPixel = ivec2(historyPixel) + offsets[i];
        float confidence = 1.0;

        uint historyMaterialIdx = texelFetch(historyMaterialIdxTexture, offsetPixel, 0).r;
        confidence *= historyMaterialIdx != materialIdx ? 0.0 : 1.0;

        vec3 historyNormal = DecodeNormal(texelFetch(historyNormalTexture, offsetPixel, 0).rg);
        confidence *= pow(abs(dot(historyNormal, normal)), 2.0);

        float historyDepth = texelFetch(historyDepthTexture, offsetPixel, 0).r;
        float historyLinearDepth = ConvertDepthToViewSpaceDepth(historyDepth);
        confidence *= min(1.0 , exp(-abs(linearDepth - historyLinearDepth)));

        if (confidence > 0.1) {
            totalWeight += 1.0;
            history += texelFetch(historyTexture, offsetPixel, 0);
            historyMoments += texelFetch(historyMomentsTexture, offsetPixel, 0);
        }
    }

    if (totalWeight > 0.0) {
        history /= totalWeight;
        historyMoments /= totalWeight;
        return true;
    }

    history = vec4(0.0);
    historyMoments = vec4(0.0);

    return false;

}

void SampleCatmullRom(ivec2 pixel, vec2 uv, out vec4 history) {

    // http://advances.realtimerendering.com/s2016/Filmic%20SMAA%20v7.pptx
    // Credit: Jorge Jimenez (SIGGRAPH 2016)
    // Ignores the 4 corners of the 4x4 grid
    // Learn more: http://vec3.ca/bicubic-filtering-in-fewer-taps/
    vec2 position = uv * resolution;

    vec2 center = floor(position - 0.5) + 0.5;
    vec2 f = position - center;
    vec2 f2 = f * f;
    vec2 f3 = f2 * f;

    vec2 w0 = f2 - 0.5 * (f3 + f);
    vec2 w1 = 1.5 * f3 - 2.5 * f2 + 1.0;
    vec2 w3 = 0.5 * (f3 - f2);
    vec2 w2 = 1.0 - w0 - w1 - w3;

    vec2 w12 = w1 + w2;

    vec2 tc0 = (center - 1.0) * invResolution;
    vec2 tc12 = (center + w2 / w12) * invResolution;
    vec2 tc3 = (center + 2.0) * invResolution;

    vec2 uv0 = clamp(vec2(tc12.x, tc0.y), vec2(0.0), vec2(1.0)) * resolution - vec2(0.5);
    vec2 uv1 = clamp(vec2(tc0.x, tc12.y), vec2(0.0), vec2(1.0)) * resolution - vec2(0.5);
    vec2 uv2 = clamp(vec2(tc12.x, tc12.y), vec2(0.0), vec2(1.0)) * resolution - vec2(0.5);
    vec2 uv3 = clamp(vec2(tc3.x, tc12.y), vec2(0.0), vec2(1.0)) * resolution - vec2(0.5);
    vec2 uv4 = clamp(vec2(tc12.x, tc3.y), vec2(0.0), vec2(1.0)) * resolution - vec2(0.5);

    float weight0 = w12.x * w0.y;
    float weight1 = w0.x * w12.y;
    float weight2 = w12.x * w12.y;
    float weight3 = w3.x * w12.y;
    float weight4 = w12.x * w3.y;
    
    vec4 sample0, sample1, sample2, sample3, sample4;
    vec4 moments0, moments1, moments2, moments3, moments4;

    SampleHistory(pixel, uv0, sample0, moments0);
    SampleHistory(pixel, uv1, sample1, moments1);
    SampleHistory(pixel, uv2, sample2, moments2);
    SampleHistory(pixel, uv3, sample3, moments3);
    SampleHistory(pixel, uv4, sample4, moments4);

    sample0 *= weight0;
    sample1 *= weight1;
    sample2 *= weight2;
    sample3 *= weight3;
    sample4 *= weight4;

    float totalWeight = weight0 + weight1 + 
        weight2 + weight3 + weight4;

    history = sample0 + sample1 +
        sample2 + sample3 + sample4;

}

void ComputeVarianceMinMax(out vec3 mean, out vec3 std) {

    vec3 m1 = vec3(0.0);
    vec3 m2 = vec3(0.0);
    // This could be varied using the temporal variance estimation
    // By using a wide neighborhood for variance estimation (8x8) we introduce block artifacts
    // These are similiar to video compression artifacts, the spatial filter mostly clears them up
    const int radius = kernelRadius;
    ivec2 pixel = ivec2(gl_GlobalInvocationID);

    float depth = texelFetch(depthTexture, pixel, 0).r;
    float linearDepth = ConvertDepthToViewSpaceDepth(depth);

    uint materialIdx = texelFetch(materialIdxTexture, pixel, 0).r;

    float totalWeight = 0.0;

    for (int i = -radius; i <= radius; i++) {
        for (int j = -radius; j <= radius; j++) {
            int sharedMemoryIdx = GetSharedMemoryIndex(ivec2(i, j));

            vec3 sampleRadiance = FetchCurrentRadiance(sharedMemoryIdx);
            float sampleLinearDepth = FetchDepth(sharedMemoryIdx);

            float depthPhi = max(1.0, abs(0.025 * linearDepth));
            float weight = min(1.0 , exp(-abs(linearDepth - sampleLinearDepth) / depthPhi));
        
            m1 += sampleRadiance * weight;
            m2 += sampleRadiance * sampleRadiance * weight;

            totalWeight += weight;
        }
    }

    mean = m1 / totalWeight;
    std = sqrt(max((m2 / totalWeight) - (mean * mean), 0.0));
}

void main() {

    LoadGroupSharedData();

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(resolveImage).x ||
        pixel.y > imageSize(resolveImage).y)
        return;

    vec3 mean, std;
    ComputeVarianceMinMax(mean, std);

    ivec2 velocityPixel = pixel;
    vec2 velocity = texelFetch(velocityTexture, velocityPixel, 0).rg;

    vec2 uv = (vec2(pixel) + vec2(0.5)) * invResolution + velocity;
    vec2 historyPixel = vec2(pixel) + velocity * resolution;

    bool valid = true;
    vec4 history;
    vec4 historyMoments;
    valid = SampleHistory(pixel, historyPixel, history, historyMoments);

    SampleCatmullRom(pixel, uv, history);

    vec3 historyColor = history.rgb;
    vec3 currentColor = texelFetch(currentTexture, pixel, 0).rgb;

    vec2 currentMoments;
    currentMoments.r = Luma(currentColor);
    currentMoments.g = currentMoments.r * currentMoments.r;

    vec3 historyNeighbourhoodMin = mean - std;
    vec3 historyNeighbourhoodMax = mean + std;

    vec3 currentNeighbourhoodMin = mean - pushConstants.currentClipFactor * std;
    vec3 currentNeighbourhoodMax = mean + pushConstants.currentClipFactor * std;

    // In case of clipping we might also reject the sample. TODO: Investigate
    float clipBlend = ClipBoundingBox(historyNeighbourhoodMin, historyNeighbourhoodMax,
        historyColor, currentColor);
    float adjClipBlend = clamp(clipBlend, 0.0, pushConstants.historyClipMax);
    currentColor = clamp(currentColor, currentNeighbourhoodMin, currentNeighbourhoodMax);
    historyColor = mix(historyColor, currentColor, adjClipBlend);

    uint materialIdx = texelFetch(materialIdxTexture, pixel, 0).r;
    Material material = UnpackMaterial(materialIdx);

    float roughness = material.roughness;
    roughness *= material.roughnessMap ? texelFetch(roughnessMetallicAoTexture, pixel, 0).r : 1.0;

    float factor = clamp(16.0 * log(roughness + 1.0), 0.001, pushConstants.temporalWeight);
    factor = (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0
         || uv.y > 1.0) ? 0.0 : factor;

    float historyLength = historyMoments.b;
    if (factor < 0.1 * roughness || !valid) {
        historyLength = 0.0;
        currentMoments.g = 1.0;
        currentMoments.r = 0.0;
    }

    factor = min(factor, historyLength / (historyLength + 1.0));

    vec3 resolve = mix(currentColor, historyColor, factor);
    vec2 momentsResolve = mix(currentMoments, historyMoments.rg, factor);

    // Boost variance when we have a small history length (we trade blur for noise)
    float varianceBoost = max(1.0, 4.0 / (historyLength + 1.0));
    float variance = max(0.0, momentsResolve.g - momentsResolve.r * momentsResolve.r);
    variance *= varianceBoost;

    variance = roughness <= 0.02 ? 0.0 : variance;

    imageStore(momentsImage, pixel, vec4(momentsResolve, historyLength + 1.0, 0.0));
    imageStore(resolveImage, pixel, vec4(resolve, variance));

}