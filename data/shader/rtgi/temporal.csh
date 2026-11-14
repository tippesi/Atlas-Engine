#ifndef AE_OS_MACOS
#define BICUBIC_FILTER
#endif

#include <../common/utility.hsh>
#include <../common/ycocg.hsh>
#include <../common/convert.hsh>
#include <../common/PI.hsh>
#include <../common/stencil.hsh>
#include <../common/flatten.hsh>
#include <../common/material.hsh>
#include <../common/random.hsh>
#include <../common/normalencode.hsh>
#include <../common/edgePreserving.hsh>
#include <../common/denoiser.hsh>

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

const int kernelRadius = 3;

const uint sharedDataSize = (gl_WorkGroupSize.x + 2 * kernelRadius) * (gl_WorkGroupSize.y + 2 * kernelRadius);
const ivec2 unflattenedSharedDataSize = ivec2(gl_WorkGroupSize) + 2 * kernelRadius;

shared vec4 sharedRadianceDepth[sharedDataSize];
shared uint sharedMaterialIdx[sharedDataSize];
shared float sharedHitDistance[sharedDataSize];
shared vec2 sharedObjectMotion[sharedDataSize];

// Neighborhood of the encoded hit distance that is accumulated and compared with its history
const int hitDistanceRadius = 2;
// Spread of a pixel's encoded hit distance assumed in addition to the measured one,
// a few neighbors or identical values (all misses) don't give a reliable estimate
const float hitDistancePriorSpread = 0.1;

layout(push_constant) uniform constants {
    float temporalWeight;
    float historyClipMax;
    float currentClipFactor;
    int resetHistory;
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

void LoadGroupSharedData() {

    ivec2 workGroupOffset = ivec2(gl_WorkGroupID) * ivec2(gl_WorkGroupSize) - ivec2(kernelRadius);

    uint workGroupSize = gl_WorkGroupSize.x * gl_WorkGroupSize.y;
    for(uint i = gl_LocalInvocationIndex; i < sharedDataSize; i += workGroupSize) {
        ivec2 localOffset = Unflatten2D(int(i), unflattenedSharedDataSize);
        ivec2 texel = localOffset + workGroupOffset;

        texel = clamp(texel, ivec2(0), ivec2(resolution) - ivec2(1));

        vec4 current = texelFetch(currentTexture, texel, 0);
        float depth = texelFetch(depthTexture, texel, 0).r;
        sharedRadianceDepth[i].rgb = RGBToYCoCg(max(current.rgb, 0.0));
        sharedHitDistance[i] = current.a;
        sharedRadianceDepth[i].a = ConvertDepthToViewSpaceDepth(depth);
        sharedMaterialIdx[i].r = texelFetch(materialIdxTexture, texel, 0).r;
        sharedObjectMotion[i] = GetObjectMotion(velocityTexture, texel, depth, resolution);
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

uint FetchMaterialIdx(int sharedMemoryIdx) {

    return sharedMaterialIdx[sharedMemoryIdx].r;

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

// The bilinear footprint is complete when all four history texels are valid, their range limits a bicubic filter.
// The confidence is the valid part of the footprint, it shortens the history of partly valid reprojections
bool SampleHistory(ivec2 pixel, vec2 historyPixel, out vec4 history, out vec4 historyMoments,
    out bool completeFootprint, out vec4 historyMin, out vec4 historyMax, out float footprintConfidence) {

    footprintConfidence = 0.0;
    history = vec4(0.0);
    historyMoments = vec4(0.0);
    historyMin = vec4(1e30);
    historyMax = vec4(-1e30);

    int validCount = 0;
    float totalWeight = 0.0;
    float x    = fract(historyPixel.x);
    float y    = fract(historyPixel.y);

    float weights[4] = { (1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y };

    vec3 normal = DecodeNormal(texelFetch(normalTexture, pixel, 0).rg);
    float depth = texelFetch(depthTexture, pixel, 0).r;
    uint materialIdx = texelFetch(materialIdxTexture, pixel, 0).r;

    vec2 texCoord = (vec2(pixel) + 0.5) / resolution;
    vec3 position = ConvertDepth(depth, texCoord, globalData.ipvMatrixCurrent);
    vec3 worldNormal = normalize(vec3(globalData.ivMatrix * vec4(normal, 0.0)));
    
    vec3 viewDir = normalize(position - globalData.cameraLocation.xyz);
    float NdotV = abs(dot(viewDir, worldNormal));
    float depthPhi = max(1.0, NdotV * 32.0 *  abs(ConvertDepthToViewSpaceDepth(depth)));

    // Calculate confidence over 2x2 bilinear neighborhood
    for (int i = 0; i < 4; i++) {
        ivec2 offsetPixel = ivec2(historyPixel) + pixelOffsets[i];

        offsetPixel = clamp(offsetPixel, ivec2(0), ivec2(resolution) - ivec2(1));

        vec3 historyNormal = DecodeNormal(texelFetch(historyNormalTexture, offsetPixel, 0).rg);
        float normalWeight = GetEdgePreservingNormalWeight(normal, historyNormal, 1.0);

        float historyDepth = texelFetch(historyDepthTexture, offsetPixel, 0).r;
        float depthWeight = min(1.0 , exp(-abs(depth - historyDepth) * depthPhi));

        uint historyMaterialIdx = texelFetch(historyMaterialIdxTexture, offsetPixel, 0).r;
        float materialWeight = historyMaterialIdx == materialIdx ? 1.0 : 0.0;

        float confidence = normalWeight * depthWeight * materialWeight;

        if (confidence > 0.2) {
            vec4 historyTexel = texelFetch(historyTexture, offsetPixel, 0);
            historyMin = min(historyMin, historyTexel);
            historyMax = max(historyMax, historyTexel);
            validCount++;

            totalWeight += weights[i];
            history += historyTexel * weights[i];
            historyMoments += texelFetch(historyMomentsTexture, offsetPixel, 0) * weights[i];
        }
    }

    completeFootprint = validCount == 4;

    if (totalWeight > 0.0) {
        footprintConfidence = totalWeight;
        history /= totalWeight;
        historyMoments /= totalWeight;
        return true;
    }

    for (int i = 0; i < 9; i++) {
        ivec2 offsetPixel = ivec2(historyPixel) + offsets[i];

        offsetPixel = clamp(offsetPixel, ivec2(0), ivec2(resolution) - ivec2(1));

        vec3 historyNormal = DecodeNormal(texelFetch(historyNormalTexture, offsetPixel, 0).rg);
        float normalWeight = GetEdgePreservingNormalWeight(normal, historyNormal, 1.0);

        float historyDepth = texelFetch(historyDepthTexture, offsetPixel, 0).r;
        float depthWeight = min(1.0 , exp(-abs(depth - historyDepth) * depthPhi));

        uint historyMaterialIdx = texelFetch(historyMaterialIdxTexture, offsetPixel, 0).r;
        float materialWeight = historyMaterialIdx == materialIdx ? 1.0 : 0.0;

        float confidence = normalWeight * depthWeight * materialWeight;

        if (confidence > 0.5) {
            totalWeight += 1.0;
            history += texelFetch(historyTexture, offsetPixel, 0);
            historyMoments += texelFetch(historyMomentsTexture, offsetPixel, 0);
        }
    }

    if (totalWeight > 0.0) {
        // None of the footprint texels matched, the neighbors around it are less trustworthy
        footprintConfidence = totalWeight / 9.0;
        history /= totalWeight;
        historyMoments /= totalWeight;
        return true;
    }

    history = vec4(0.0);
    historyMoments = vec4(0.0);

    return false;

}

// Sample counts are effective ones of the weighted neighborhoods, (sum of weights)^2 / sum of squared weights.
// The hit distance mean comes with its standard error, the noise expected for it without any change.
// The nearby motion is the fastest movement of a neighbor around the same depth relative to the pixel in pixels
void ComputeVarianceMinMax(out vec3 mean, out vec3 std, out float sampleCount,
    out float hitDistanceMean, out float hitDistanceError, out float nearbyMotion) {

    vec3 m1 = vec3(0.0);
    vec3 m2 = vec3(0.0);
    float squaredWeight = 0.0;

    float h1 = 0.0;
    float h2 = 0.0;
    float hitDistanceWeight = 0.0;
    float hitDistanceSquaredWeight = 0.0;

    // This could be varied using the temporal variance estimation
    // By using a wide neighborhood for variance estimation (8x8) we introduce block artifacts
    // These are similiar to video compression artifacts, the spatial filter mostly clears them up
    const int radius = kernelRadius;
    ivec2 pixel = ivec2(gl_GlobalInvocationID);

    float depth = texelFetch(depthTexture, pixel, 0).r;
    vec2 texCoord = (vec2(pixel) + 0.5) / resolution;
    vec3 position = ConvertDepth(depth, texCoord, globalData.ipvMatrixCurrent);
    vec3 normal = vec3(globalData.ivMatrix * vec4(DecodeNormal(texelFetch(normalTexture, pixel, 0).rg), 0.0));

    vec3 viewDir = normalize(position - globalData.cameraLocation.xyz);
    float NdotV = abs(dot(viewDir, normal));
    float linearDepth = ConvertDepthToViewSpaceDepth(depth);
    float depthPhi = max(1.0, NdotV * 16.0 / max(1.0, abs(linearDepth)));

    uint materialIdx = texelFetch(materialIdxTexture, pixel, 0).r;

    float totalWeight = 0.0;

    vec2 centerObjectMotion = sharedObjectMotion[GetSharedMemoryIndex(ivec2(0))];
    float nearbyMotionSquared = 0.0;
    float depthScale = 1.442695 * depthPhi;

    for (int i = -radius; i <= radius; i++) {
        for (int j = -radius; j <= radius; j++) {

            int sharedMemoryIdx = GetSharedMemoryIndex(ivec2(i, j));

            vec3 sampleRadiance = FetchCurrentRadiance(sharedMemoryIdx);
            float sampleLinearDepth = FetchDepth(sharedMemoryIdx);
            uint sampleMaterialIdx = FetchMaterialIdx(sharedMemoryIdx);

            float weight = exp2(-abs(linearDepth - sampleLinearDepth) * depthScale);

            weight *= sampleMaterialIdx != materialIdx ? 0.0 : 1.0;
        
            m1 += sampleRadiance * weight;
            m2 += sampleRadiance * sampleRadiance * weight;

            totalWeight += weight;
            squaredWeight += weight * weight;

            nearbyMotionSquared = max(nearbyMotionSquared, GetNearbyMotionSquared(sharedObjectMotion[sharedMemoryIdx],
                centerObjectMotion, linearDepth, sampleLinearDepth));

            if (abs(i) <= hitDistanceRadius && abs(j) <= hitDistanceRadius) {
                float sampleHitDistance = sharedHitDistance[sharedMemoryIdx];
                h1 += sampleHitDistance * weight;
                h2 += sampleHitDistance * sampleHitDistance * weight;
                hitDistanceWeight += weight;
                hitDistanceSquaredWeight += weight * weight;
            }
        }
    }

    mean = m1 / totalWeight;
    std = sqrt(max((m2 / totalWeight) - (mean * mean), 0.0));
    sampleCount = sqr(totalWeight) / squaredWeight;
    nearbyMotion = sqrt(nearbyMotionSquared);

    hitDistanceMean = h1 / hitDistanceWeight;
    float hitDistanceSampleCount = sqr(hitDistanceWeight) / hitDistanceSquaredWeight;
    float hitDistanceVariance = max(h2 / hitDistanceWeight - hitDistanceMean * hitDistanceMean, 0.0) *
        hitDistanceSampleCount / max(hitDistanceSampleCount - 1.0, 1.0);
    hitDistanceError = sqrt((hitDistanceVariance + sqr(hitDistancePriorSpread)) / hitDistanceSampleCount);
}

void main() {

    LoadGroupSharedData();

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x >= imageSize(resolveImage).x ||
        pixel.y >= imageSize(resolveImage).y)
        return;

    vec3 mean, std;
    float sampleCount, hitDistanceMean, hitDistanceError, nearbyMotion;
    ComputeVarianceMinMax(mean, std, sampleCount, hitDistanceMean, hitDistanceError, nearbyMotion);

    ivec2 velocityPixel = pixel;
    vec2 velocity = texelFetch(velocityTexture, velocityPixel, 0).rg;

    vec2 uv = (vec2(pixel) + vec2(0.5)) * invResolution + velocity;
    vec2 historyPixel = vec2(pixel) + (velocity * resolution);

    vec4 history;
    vec4 historyMoments;
    bool completeFootprint;
    vec4 historyMin, historyMax;
    float footprintConfidence;
    bool valid = SampleHistory(pixel, historyPixel, history, historyMoments,
        completeFootprint, historyMin, historyMax, footprintConfidence);

#ifdef BICUBIC_FILTER
    // The clamp removes ringing and texels outside the footprint that belong to other surfaces
    if (completeFootprint)
        history = clamp(SampleCatmullRom(historyTexture, uv, resolution), historyMin, historyMax);
#endif

    // The history was stored with the pre-exposure of the last frame, the moments hold luma and its square
    float exposureScale = globalData.preExposure / globalData.lastPreExposure;
    history.rgb *= exposureScale;
    historyMoments.rg *= vec2(exposureScale, exposureScale * exposureScale);

    vec3 historyColor = RGBToYCoCg(history.rgb);
    vec3 currentColor = RGBToYCoCg(texelFetch(currentTexture, pixel, 0).rgb);

    vec2 currentMoments;
    currentMoments.r = currentColor.r;
    currentMoments.g = currentMoments.r * currentMoments.r;

    float historyLength = historyMoments.b * footprintConfidence;

    // A changed hit distance means the geometry the rays see changed, shorten above two standard errors, reset at six
    float hitDistanceDeviation = abs(hitDistanceMean - historyMoments.a) / hitDistanceError;
    historyLength *= saturate((6.0 - hitDistanceDeviation) / 4.0);

    historyLength = LimitHistoryByNearbyMotion(historyLength, nearbyMotion);

    // Two standard deviations, a narrower box shortens converged histories at highlights and edges
    vec3 historyNeighbourhoodMin = mean - 2.0 * std;
    vec3 historyNeighbourhoodMax = mean + 2.0 * std;

    vec3 currentNeighbourhoodMin = mean - pushConstants.currentClipFactor * std;
    vec3 currentNeighbourhoodMax = mean + pushConstants.currentClipFactor * std;

    // Histories outside the neighborhood shorten, small neighborhoods (thin objects, edges) fully from ten samples on
    float clipBlend = ClipBoundingBox(historyNeighbourhoodMin, historyNeighbourhoodMax,
        historyColor, currentColor);
    float adjClipBlend = clamp(clipBlend, 0.0, pushConstants.historyClipMax);
    historyLength *= 1.0 - adjClipBlend * saturate((sampleCount - 2.0) / 8.0);
    currentColor = clamp(currentColor, currentNeighbourhoodMin, currentNeighbourhoodMax);

    if (IsOffscreen(uv) || pushConstants.resetHistory > 0 || !valid) {
        historyLength = 0.0;
        currentMoments.g = 1.0;
        currentMoments.r = 0.0;
    }

    // New and short histories (disocclusions) start from the neighborhood mean instead of single noisy samples
    currentColor = mix(mean, currentColor, saturate(historyLength / 4.0));

    historyColor = YCoCgToRGB(historyColor);
    currentColor = YCoCgToRGB(currentColor);

    float factor = min(pushConstants.temporalWeight, historyLength / (historyLength + 1.0));
    
    vec3 resolve = factor <= 0.0 ? currentColor : mix(currentColor, historyColor, factor);
    vec2 momentsResolve = factor <= 0.0 ? currentMoments : mix(currentMoments, historyMoments.rg, factor);

    float hitDistance = factor <= 0.0 ? hitDistanceMean : mix(hitDistanceMean, historyMoments.a, factor);

    float accumulatedFrames = GetAccumulatedFrames(factor, historyLength);

    // Boost variance when we have a small history length (we trade blur for noise)
    float varianceBoost = max(1.0, 16.0 / accumulatedFrames);
    float variance = max(0.0, momentsResolve.g - momentsResolve.r * momentsResolve.r);
    variance *= varianceBoost;

    variance = max(variance, 1e-5);

    imageStore(momentsImage, pixel, vec4(momentsResolve, accumulatedFrames, hitDistance));
    imageStore(resolveImage, pixel, vec4(resolve, variance));

}