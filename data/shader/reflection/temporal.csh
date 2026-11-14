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
layout(set = 3, binding = 13) uniform sampler2D historyRoughnessMetallicAoTexture;

vec2 invResolution = 1.0 / vec2(imageSize(resolveImage));
vec2 resolution = vec2(imageSize(resolveImage));

const int kernelRadius = 3;

const uint sharedDataSize = (gl_WorkGroupSize.x + 2 * kernelRadius) * (gl_WorkGroupSize.y + 2 * kernelRadius);
const ivec2 unflattenedSharedDataSize = ivec2(gl_WorkGroupSize) + 2 * kernelRadius;

shared vec4 sharedRadianceDepth[sharedDataSize];
shared vec2 sharedObjectMotion[sharedDataSize];

layout(push_constant) uniform constants {
    float temporalWeight;
    float historyClipMax;
    float currentClipFactor;
    float roughnessCutoff;
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

vec4 FetchTexel(ivec2 texel) {
    
    vec4 color = texelFetch(currentTexture, texel, 0);
    color.rgb = max(color.rgb, 0);
    return color;

}

void LoadGroupSharedData() {

    ivec2 workGroupOffset = ivec2(gl_WorkGroupID) * ivec2(gl_WorkGroupSize) - ivec2(kernelRadius);

    uint workGroupSize = gl_WorkGroupSize.x * gl_WorkGroupSize.y;
    for(uint i = gl_LocalInvocationIndex; i < sharedDataSize; i += workGroupSize) {
        ivec2 localOffset = Unflatten2D(int(i), unflattenedSharedDataSize);
        ivec2 texel = localOffset + workGroupOffset;

        texel = clamp(texel, ivec2(0), ivec2(resolution) - ivec2(1));

        vec4 radianceRayLength = FetchTexel(texel);
        float depth = texelFetch(depthTexture, texel, 0).r;
        sharedRadianceDepth[i].rgb = RGBToYCoCg(radianceRayLength.rgb);
        sharedRadianceDepth[i].a = ConvertDepthToViewSpaceDepth(depth);
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

vec3 GetHistoryWorldPosition(ivec2 historyPixel) {

    float historyDepth = texelFetch(historyDepthTexture, historyPixel, 0).r;
    vec2 ndc = (vec2(historyPixel) + 0.5) * invResolution * 2.0 - 1.0;
    vec4 position = globalData.ipvMatrixLast * vec4(ndc, historyDepth, 1.0);
    return position.xyz / position.w;

}

// The bilinear footprint is complete when all four history texels are valid, their range limits a bicubic filter.
// The confidence is the valid part of the footprint, it shortens the history of partly valid reprojections
bool SampleHistory(ivec2 pixel, vec2 historyPixel, float normalPhi, out vec4 history, out vec4 historyMoments,
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

    vec3 viewPosition = ConvertDepthToViewSpace(depth, (vec2(pixel) + 0.5) * invResolution);
    vec3 position = vec3(globalData.ivMatrix * vec4(viewPosition, 1.0));
    vec3 worldNormal = normalize(mat3(globalData.ivMatrix) * normal);
    // The distance to the surface plane stays stable at grazing angles, unlike the depth
    float depthPhi = normalPhi / abs(viewPosition.z);

    float roughness = texelFetch(roughnessMetallicAoTexture, pixel, 0).r;
    uint materialIdx = texelFetch(materialIdxTexture, pixel, 0).r;

    // Calculate confidence over 2x2 bilinear neighborhood
    for (int i = 0; i < 4; i++) {
        ivec2 offsetPixel = ivec2(historyPixel) + pixelOffsets[i];
        float confidence = 1.0;

        offsetPixel = clamp(offsetPixel, ivec2(0), ivec2(resolution) - ivec2(1));

        vec3 historyNormal = DecodeNormal(texelFetch(historyNormalTexture, offsetPixel, 0).rg);
        confidence *= pow(max(dot(historyNormal, normal), 0.0), normalPhi);

        float planeDistance = abs(dot(GetHistoryWorldPosition(offsetPixel) - position, worldNormal));
        confidence *= min(1.0 , exp(-planeDistance * depthPhi));

        uint historyMaterialIdx = texelFetch(historyMaterialIdxTexture, offsetPixel, 0).r;
        confidence *= historyMaterialIdx == materialIdx ? 1.0 : 0.0;

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
        ivec2 offsetPixel = ivec2(historyPixel + 0.5) + offsets[i];
        float confidence = 1.0;

        offsetPixel = clamp(offsetPixel, ivec2(0), ivec2(resolution) - ivec2(1));

        vec3 historyNormal = DecodeNormal(texelFetch(historyNormalTexture, offsetPixel, 0).rg);
        confidence *= pow(max(dot(historyNormal, normal), 0.0), normalPhi);

        float planeDistance = abs(dot(GetHistoryWorldPosition(offsetPixel) - position, worldNormal));
        confidence *= min(1.0 , exp(-planeDistance * depthPhi));

        uint historyMaterialIdx = texelFetch(historyMaterialIdxTexture, offsetPixel, 0).r;
        confidence *= historyMaterialIdx == materialIdx ? 1.0 : 0.0;

        if (confidence > 0.2) {
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

// The sample count is the effective one of the weighted neighborhood, (sum of weights)^2 / sum of squared weights.
// The nearby motion is the fastest movement of a neighbor around the same depth relative to the pixel in pixels
void ComputeVarianceMinMax(int radius, out vec3 mean, out vec3 std, out float sampleCount, out float nearbyMotion) {

    vec3 m1 = vec3(0.0);
    vec3 m2 = vec3(0.0);
    float squaredWeight = 0.0;
    // This could be varied using the temporal variance estimation
    // By using a wide neighborhood for variance estimation (8x8) we introduce block artifacts
    // These are similiar to video compression artifacts, the spatial filter mostly clears them up
    
    ivec2 pixel = ivec2(gl_GlobalInvocationID);

    float depth = texelFetch(depthTexture, pixel, 0).r;
    float linearDepth = ConvertDepthToViewSpaceDepth(depth);
    float depthPhi = max(1.0, abs(0.025 * linearDepth));

    float totalWeight = 0.0;

    vec2 centerObjectMotion = sharedObjectMotion[GetSharedMemoryIndex(ivec2(0))];
    float nearbyMotionSquared = 0.0;
    float depthScale = 1.442695 / depthPhi;

    for (int i = -radius; i <= radius; i++) {
        for (int j = -radius; j <= radius; j++) {
            int sharedMemoryIdx = GetSharedMemoryIndex(ivec2(i, j));

            vec3 sampleRadiance = FetchCurrentRadiance(sharedMemoryIdx);
            float sampleLinearDepth = FetchDepth(sharedMemoryIdx);

            float weight = exp2(-abs(linearDepth - sampleLinearDepth) * depthScale);

            m1 += sampleRadiance * weight;
            m2 += sampleRadiance * sampleRadiance * weight;

            totalWeight += weight;
            squaredWeight += weight * weight;

            nearbyMotionSquared = max(nearbyMotionSquared, GetNearbyMotionSquared(sharedObjectMotion[sharedMemoryIdx],
                centerObjectMotion, linearDepth, sampleLinearDepth));
        }
    }

    mean = m1 / totalWeight;
    std = sqrt(max((m2 / totalWeight) - (mean * mean), 0.0));
    sampleCount = sqr(totalWeight) / squaredWeight;
    nearbyMotion = sqrt(nearbyMotionSquared);
}

float GetSpecularDominantFactor(float NdotV, float roughness) {

    float a =  0.298475 * log(39.4115 - 39.0029 * roughness);
    float f = pow(saturate(1.0 - NdotV), 10.8649) * (1.0 - a) + a;

    return sqr(saturate(f));

}

float GetFilteredRayLength(ivec2 pixel) {

    float centerDepth = FetchDepth(GetSharedMemoryIndex(ivec2(0)));
    float depthPhi = 32.0 / abs(centerDepth);

    float rayLength = 0.0;
    float totalWeight = 0.0;

    // Misses have a ray length of zero and are excluded
    for (int i = 0; i < 9; i++) {
        ivec2 samplePixel = clamp(pixel + offsets[i], ivec2(0), ivec2(resolution) - ivec2(1));
        float sampleRayLength = abs(texelFetch(currentTexture, samplePixel, 0).a);
        float sampleDepth = FetchDepth(GetSharedMemoryIndex(offsets[i]));

        float weight = sampleRayLength > 0.0 ? min(1.0, exp(-abs(centerDepth - sampleDepth) * depthPhi)) : 0.0;
        rayLength += sampleRayLength * weight;
        totalWeight += weight;
    }

    return totalWeight > 0.0 ? rayLength / totalWeight : 0.0;

}

vec2 VirtualPointReprojection(vec2 pixel, ivec2 size, float rayLength) {

    const vec2 texCoord  = (vec2(pixel)) / vec2(size);   
    vec3 rayOrigin = vec3(globalData.ivMatrix * vec4(ConvertDepthToViewSpace(texelFetch(depthTexture, ivec2(pixel), 0).r, texCoord), 1.0));

    vec3 cameraRay = rayOrigin - globalData.cameraLocation.xyz;

    float cameraRayLength = length(cameraRay);
    float reflectionRayLength = rayLength;

    cameraRay = normalize(cameraRay);

    vec3 parallaxHitPoint = globalData.cameraLocation.xyz + cameraRay * (cameraRayLength + reflectionRayLength);

    vec4 parallaxHitPointLast = globalData.pMatrix * globalData.vMatrixLast * vec4(parallaxHitPoint, 1.0);
    parallaxHitPointLast.xy /= parallaxHitPointLast.w;
 
    return (parallaxHitPointLast.xy * 0.5 + 0.5) * vec2(size);
    
}

void main() {

    LoadGroupSharedData();

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x >= imageSize(resolveImage).x ||
        pixel.y >= imageSize(resolveImage).y)
        return;

    vec2 uv = (vec2(pixel) + 0.5) * invResolution;

    vec2 velocity = texelFetch(velocityTexture, pixel, 0).rg;
    float roughness = textureLod(roughnessMetallicAoTexture, uv, .0).r;
    vec3 normal = normalize(DecodeNormal(texelFetch(normalTexture, pixel, 0).rg));

    float depth = texelFetch(depthTexture, pixel, 0).r;
    vec3 position = ConvertDepthToViewSpace(depth, uv);

    float NdotV = abs(dot(normalize(-position), normal));

    float dominantFactor = GetSpecularDominantFactor(NdotV, sqrt(roughness));

    vec3 mean, std;
    float sampleCount, nearbyMotion;
#ifdef UPSCALE
    const int radius = int(mix(3.0, float(kernelRadius), min(1.0, roughness * 4.0)));
#else
    const int radius = int(mix(2.0, float(kernelRadius), min(1.0, roughness * 4.0)));
#endif
    // Above the cutoff the probes don't need much denoising (except for roughness = 1. looks noisy for metallic surfaces)
    ComputeVarianceMinMax(roughness < pushConstants.roughnessCutoff ? radius : 3,
        mean, std, sampleCount, nearbyMotion);

    vec2 historyPixel = vec2(pixel) + velocity * resolution;
    vec2 historyUv = (historyPixel + 0.5) * invResolution;

    vec4 history;
    vec4 historyMoments;
    bool completeFootprint;
    vec4 historyMin, historyMax;
    float footprintConfidence;
    bool valid = SampleHistory(pixel, historyPixel, 16.0,
        history, historyMoments, completeFootprint, historyMin, historyMax, footprintConfidence) && !IsOffscreen(historyUv);

#ifdef BICUBIC_FILTER
    // The clamp removes ringing and texels outside the footprint that belong to other surfaces
    if (roughness < pushConstants.roughnessCutoff && completeFootprint)
        history = clamp(SampleCatmullRom(historyTexture, historyUv, resolution), historyMin, historyMax);
#endif

    // The history was stored with the pre-exposure of the last frame, the moments hold luma and its square
    float exposureScale = globalData.preExposure / globalData.lastPreExposure;
    vec2 momentsExposureScale = vec2(exposureScale, exposureScale * exposureScale);
    history.rgb *= exposureScale;
    historyMoments.rg *= momentsExposureScale;

    vec3 currentColor = RGBToYCoCg(texelFetch(currentTexture, pixel, 0).rgb);
    
    // Misses have a ray length of zero. Mirrors use their own ray length, it has no noise and averaging it across
    // the edges of reflected objects would mix their distances
    const float virtualCutoff = 0.2;
    float rayLength = texelFetch(currentTexture, pixel, 0).a;
    float filteredRayLength = mix(abs(rayLength), GetFilteredRayLength(pixel), saturate(roughness / virtualCutoff));
    if (rayLength != 0 &&  roughness < pushConstants.roughnessCutoff) {
        vec2 virtualHistoryPixel = VirtualPointReprojection(pixel, imageSize(resolveImage), filteredRayLength);
        vec2 virtualHistoryUv = (virtualHistoryPixel + 0.5) * invResolution;

        vec4 historyVirtualRepojection;
        vec4 historyMomentsVirtualRepojection;
        bool completeVirtualFootprint;
        vec4 virtualHistoryMin, virtualHistoryMax;
        float virtualFootprintConfidence;
        bool validRepojection = SampleHistory(pixel, virtualHistoryPixel, 32.0,
            historyVirtualRepojection, historyMomentsVirtualRepojection,
            completeVirtualFootprint, virtualHistoryMin, virtualHistoryMax, virtualFootprintConfidence) &&
            !IsOffscreen(virtualHistoryUv);

#ifdef BICUBIC_FILTER
        if (validRepojection && completeVirtualFootprint)
            historyVirtualRepojection = clamp(SampleCatmullRom(historyTexture, virtualHistoryUv, resolution),
                virtualHistoryMin, virtualHistoryMax);
#endif
        historyVirtualRepojection.rgb *= exposureScale;
        historyMomentsVirtualRepojection.rg *= momentsExposureScale;

        ivec2 virtualHistoryTexel = clamp(ivec2(virtualHistoryPixel), ivec2(0), ivec2(resolution) - ivec2(1));
        vec3 virtualNormal = normalize(DecodeNormal(texelFetch(historyNormalTexture, virtualHistoryTexel, 0).rg));
        float virtualRoughness = texelFetch(historyRoughnessMetallicAoTexture, virtualHistoryTexel, 0).r;

        float virtualConfidence = dominantFactor * sqr(max((virtualCutoff - roughness) / virtualCutoff, 0.0));
        virtualConfidence *= pow(max(dot(virtualNormal, normal), 0.0), 512.0);
        virtualConfidence *= pow(1.0 - abs(virtualRoughness - roughness), 32.0);

        // Only valid reprojections contribute, below the virtual cutoff the virtual one replaces an invalid surface one
        float virtualWeight = validRepojection ? (valid ? virtualConfidence : (roughness < virtualCutoff ? 1.0 : 0.0)) : 0.0;
        valid = valid || virtualWeight > 0.0;
        history = mix(history, historyVirtualRepojection, virtualWeight);
        historyMoments = mix(historyMoments, historyMomentsVirtualRepojection, virtualWeight);
        footprintConfidence = mix(footprintConfidence, virtualFootprintConfidence, virtualWeight);
    }

    vec2 currentMoments;
    currentMoments.r = currentColor.r;
    currentMoments.g = currentMoments.r * currentMoments.r;

    // Two standard deviations, a narrower box shortens converged histories at highlights and reflected edges
    vec3 historyNeighbourhoodMin = mean - 2.0 * std;
    vec3 historyNeighbourhoodMax = mean + 2.0 * std;

    vec3 currentNeighbourhoodMin = mean - pushConstants.currentClipFactor * std;
    vec3 currentNeighbourhoodMax = mean + pushConstants.currentClipFactor * std;
    currentColor = clamp(currentColor, currentNeighbourhoodMin, currentNeighbourhoodMax);

    vec3 historyColor = RGBToYCoCg(history.rgb);

    float historyLength = historyMoments.b * footprintConfidence;

    // Histories outside the neighborhood shorten, small neighborhoods (thin objects, edges) fully from ten samples on
    float clipBlend = ClipBoundingBox(historyNeighbourhoodMin, historyNeighbourhoodMax,
        historyColor, currentColor);
    float adjClipBlend = clamp(clipBlend, 0.0, pushConstants.historyClipMax);
    historyLength *= 1.0 - adjClipBlend * saturate((sampleCount - 2.0) / 8.0);

    historyLength = LimitHistoryByNearbyMotion(historyLength, nearbyMotion);

    if (pushConstants.resetHistory > 0 || !valid) {
        valid = false;
        historyLength = 0.0;
        currentMoments.g = 1.0;
        currentMoments.r = 0.0;
    }

    // New and short histories (disocclusions) start from the neighborhood mean instead of single noisy samples.
    // Mirrors keep their sample, it has no noise and the mean would mix different reflected content
    currentColor = mix(mean, currentColor, max(saturate(historyLength / 4.0), 1.0 - saturate(roughness / virtualCutoff)));

    historyColor = YCoCgToRGB(historyColor);
    currentColor = YCoCgToRGB(currentColor);

    float factor = min(pushConstants.temporalWeight, historyLength / (historyLength + 1.0));

#ifdef UPSCALE
    // Misses have a ray length of zero, so the upsampling marks pixels it filled in by their position
    bool traced = pixel % 2 == pixelOffsets[globalData.frameCount % 4];
    factor = traced ? factor : (valid ? min(1.0 - adjClipBlend, historyLength / (historyLength + 1.0)) : factor);
#endif

    vec3 resolve = factor <= 0.0 ? currentColor : mix(currentColor, historyColor, factor);
    vec2 momentsResolve = factor <= 0.0 ? currentMoments : mix(currentMoments, historyMoments.rg, factor);

    // Accumulated for the spatial filter, which limits its radius by the lobe the reflection rays cover
    // Pixels above the cutoff sample the probes and have no ray length
    bool traceRay = roughness < pushConstants.roughnessCutoff && filteredRayLength > 0.0;
    float currentHitDistance = traceRay ? EncodeHitDistance(filteredRayLength) : 1.0;
    float hitDistance = factor <= 0.0 ? currentHitDistance : mix(currentHitDistance, historyMoments.a, factor);

    float accumulatedFrames = GetAccumulatedFrames(factor, historyLength);

    // Boost variance when we have a small history length (we trade blur for noise)
    float varianceBoost = max(1.0, 4.0 / accumulatedFrames);
    float variance = max(0.0, momentsResolve.g - momentsResolve.r * momentsResolve.r);
    variance *= varianceBoost;

    variance = roughness <= 0.1 ? max(variance * roughness, roughness * 1e-6) : variance;

    imageStore(momentsImage, pixel, vec4(momentsResolve, accumulatedFrames, hitDistance));
    imageStore(resolveImage, pixel, vec4(vec3(resolve), variance));

}