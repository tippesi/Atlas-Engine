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

const int kernelRadius = 5;

const uint sharedDataSize = (gl_WorkGroupSize.x + 2 * kernelRadius) * (gl_WorkGroupSize.y + 2 * kernelRadius);
const ivec2 unflattenedSharedDataSize = ivec2(gl_WorkGroupSize) + 2 * kernelRadius;

shared vec4 sharedRadianceDepth[sharedDataSize];

layout(push_constant) uniform constants {
    vec4 cameraLocationLast;
    float temporalWeight;
    float historyClipMax;
    float currentClipFactor;
    float roughnessCutoff;
    int resetHistory;
    uint frameCount;
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
        sharedRadianceDepth[i].rgb = RGBToYCoCg(radianceRayLength.rgb);
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

bool SampleHistory(ivec2 pixel, vec2 historyPixel, float normalPhi, out vec4 history, out vec4 historyMoments) {
    
    history = vec4(0.0);
    historyMoments = vec4(0.0);

    float totalWeight = 0.0;
    float x    = fract(historyPixel.x);
    float y    = fract(historyPixel.y);

    float weights[4] = { (1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y };

    vec3 normal = DecodeNormal(texelFetch(normalTexture, pixel, 0).rg);
    float depth = texelFetch(depthTexture, pixel, 0).r;

    float linearDepth = ConvertDepthToViewSpaceDepth(depth);
    float depthPhi = normalPhi / abs(linearDepth);

    float roughness = texelFetch(roughnessMetallicAoTexture, pixel, 0).r;
    uint materialIdx = texelFetch(materialIdxTexture, pixel, 0).r;

    // Calculate confidence over 2x2 bilinear neighborhood
    for (int i = 0; i < 4; i++) {
        ivec2 offsetPixel = ivec2(historyPixel) + pixelOffsets[i];
        float confidence = 1.0;

        offsetPixel = clamp(offsetPixel, ivec2(0), ivec2(resolution) - ivec2(1));

        vec3 historyNormal = DecodeNormal(texelFetch(historyNormalTexture, offsetPixel, 0).rg);
        confidence *= pow(max(dot(historyNormal, normal), 0.0), normalPhi);

        float historyDepth = texelFetch(historyDepthTexture, offsetPixel, 0).r;
        float historyLinearDepth = historyDepth;
        
        confidence *= min(1.0 , exp(-abs(linearDepth - historyLinearDepth) * depthPhi));

        uint historyMaterialIdx = texelFetch(historyMaterialIdxTexture, offsetPixel, 0).r;
        confidence *= historyMaterialIdx == materialIdx ? 1.0 : 0.0;

        if (confidence > 0.2) {
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
        ivec2 offsetPixel = ivec2(historyPixel + 0.5) + offsets[i];
        float confidence = 1.0;

        offsetPixel = clamp(offsetPixel, ivec2(0), ivec2(resolution) - ivec2(1));

        vec3 historyNormal = DecodeNormal(texelFetch(historyNormalTexture, offsetPixel, 0).rg);
        confidence *= pow(max(dot(historyNormal, normal), 0.0), normalPhi);

        float historyDepth = texelFetch(historyDepthTexture, offsetPixel, 0).r;
        float historyLinearDepth = ConvertDepthToViewSpaceDepth(historyDepth);
        confidence *= min(1.0 , exp(-abs(linearDepth - historyLinearDepth) * depthPhi));

        uint historyMaterialIdx = texelFetch(historyMaterialIdxTexture, offsetPixel, 0).r;
        confidence *= historyMaterialIdx == materialIdx ? 1.0 : 0.0;

        if (confidence > 0.2) {
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

float IsHistoryPixelValid(ivec2 pixel, float linearDepth, vec3 normal) {

    float confidence = 1.0;

    vec3 historyNormal = DecodeNormal(texelFetch(historyNormalTexture, pixel, 0).rg);
    //confidence *= pow(max(dot(historyNormal, normal), 0.0), 16.0);

    float depthPhi = 16.0 / abs(linearDepth);
    float historyDepth = texelFetch(historyDepthTexture, pixel, 0).r;
    float historyLinearDepth = historyDepth;
    confidence *= min(1.0 , exp(-abs(linearDepth - historyLinearDepth) * depthPhi));
    
    return confidence > 0.1 ? 1.0 : 0.0;

}

vec4 GetCatmullRomSample(ivec2 pixel, inout float weight, float linearDepth, vec3 normal) {

    pixel = clamp(pixel, ivec2(0), ivec2(imageSize(resolveImage) - 1));

    weight *= IsHistoryPixelValid(pixel, linearDepth, normal);

    return texelFetch(historyTexture, pixel, 0) * weight;

}

bool SampleCatmullRom(ivec2 pixel, vec2 uv, out vec4 history) {
    
    vec3 normal = DecodeNormal(texelFetch(normalTexture, pixel, 0).rg);
    float depth = texelFetch(depthTexture, pixel, 0).r;

    vec2 position = uv * resolution;

    vec2 center = floor(position - 0.5) + 0.5;
    vec2 f = position - center;
    vec2 f2 = f * f;
    vec2 f3 = f2 * f;

    vec2 w0 = f2 - 0.5 * (f3 + f);
    vec2 w1 = 1.5 * f3 - 2.5 * f2 + 1.0;
    vec2 w3 = 0.5 * (f3 - f2);
    vec2 w2 = 1.0 - w0 - w1 - w3;

    ivec2 uv0 = ivec2(center - 1.0);
    ivec2 uv1 = ivec2(center);
    ivec2 uv2 = ivec2(center + 1.0);
    ivec2 uv3 = ivec2(center + 2.0);

    ivec2 uvs[4] = { uv0, uv1, uv2, uv3 };
    vec2 weights[4] = { w0, w1, w2, w3 };

    history = vec4(0.0);

    float totalWeight = 0.0;

    for (int x = 0; x <= 3; x++) {
        for (int y = 0; y <= 3; y++) {
            float weight = weights[x].x * weights[y].y;
            ivec2 uv = ivec2(uvs[x].x, uvs[y].y);

            history += GetCatmullRomSample(uv, weight, depth, normal);
            totalWeight += weight;
        }
    }
    
    if (totalWeight > 0.5) {
        history /= totalWeight;
        history = max(history, 0.0);
   
        return true;
    }

    return false;
}

void ComputeVarianceMinMax(float roughness, int radius, out vec3 mean, out vec3 std) {

    vec3 m1 = vec3(0.0);
    vec3 m2 = vec3(0.0);
    // This could be varied using the temporal variance estimation
    // By using a wide neighborhood for variance estimation (8x8) we introduce block artifacts
    // These are similiar to video compression artifacts, the spatial filter mostly clears them up
    
    ivec2 pixel = ivec2(gl_GlobalInvocationID);

    float depth = texelFetch(depthTexture, pixel, 0).r;
    float linearDepth = ConvertDepthToViewSpaceDepth(depth);
    float depthPhi = max(1.0, abs(0.025 * linearDepth));

    uint materialIdx = texelFetch(materialIdxTexture, pixel, 0).r;

    float totalWeight = 0.0;

    for (int i = -radius; i <= radius; i++) {
        for (int j = -radius; j <= radius; j++) {
            int sharedMemoryIdx = GetSharedMemoryIndex(ivec2(i, j));

            vec3 sampleRadiance = FetchCurrentRadiance(sharedMemoryIdx);
            float sampleLinearDepth = FetchDepth(sharedMemoryIdx);
            
            float weight = min(1.0 , exp(-abs(linearDepth - sampleLinearDepth) / depthPhi));
        
            m1 += sampleRadiance * weight;
            m2 += sampleRadiance * sampleRadiance * weight;

            totalWeight += weight;
        }
    }

    mean = m1 / totalWeight;
    std = sqrt(max((m2 / totalWeight) - (mean * mean), 0.0));
}

float ComputeParallax(vec3 position, vec3 historyPosition) {

    vec3 cameraDelta = pushConstants.cameraLocationLast.xyz - globalData.cameraLocation.xyz;

    vec3 V = normalize(position);
    vec3 historyV = normalize(historyPosition - cameraDelta);

    float cosTheta = saturate(dot(V, historyV));
    return sqrt(1.0 - cosTheta * cosTheta) / max(cosTheta, 1e-6) * 60.0;

}

float GetSpecularDominantFactor(float NdotV, float roughness) {

    float a =  0.298475 * log(39.4115 - 39.0029 * roughness);
    float f = pow(saturate(1.0 - NdotV), 10.8649) * (1.0 - a) + a;

    return sqr(saturate(f));

}

float GetAccumulationSpeed(float NdotV, float roughness, float parallax) {

    const float accumCurve = 0.5;
    const float accumBasePower = 0.5;

    float acossqr = 1.0 - NdotV;
    float rsqr = sqr(roughness);

    float a = pow(saturate(acossqr), accumCurve);
    float b = 1.1 + rsqr;

    float parallaxSensitivity = (b + a) / (b - a);
    float powerScale = 1.0 + parallax * parallaxSensitivity;

    float f = 1.0 - exp2(-200.0 * rsqr);
    f *= pow(saturate(roughness), accumBasePower * powerScale);

    return min(f, pushConstants.temporalWeight);

}

vec2 SurfacePointReprojection(vec2 pixel, vec2 velocity) {

    return pixel + velocity * resolution;

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
    if (pixel.x > imageSize(resolveImage).x ||
        pixel.y > imageSize(resolveImage).y)
        return;

    vec2 uv = (vec2(pixel) + 0.5) * invResolution;

    vec2 velocity = texelFetch(velocityTexture, pixel, 0).rg;
    float roughness = textureLod(roughnessMetallicAoTexture, uv, .0).r;
    vec3 normal = normalize(DecodeNormal(texelFetch(normalTexture, pixel, 0).rg));

    vec2 surfaceHistoryPixel = SurfacePointReprojection(vec2(pixel), velocity);
    vec2 surfaceHistoryUV = (surfaceHistoryPixel + 0.5) * invResolution;

    float depth = texelFetch(depthTexture, pixel, 0).r;
    float depthHistory = texelFetch(depthTexture, ivec2(surfaceHistoryPixel), 0).r;

    //vec3 position = vec3(globalData.ivMatrix * vec4(ConvertDepthToViewSpace(depth, uv), 1.0));
    //vec3 positionHistory = vec3(inverse(globalData.vMatrixLast) * vec4(ConvertDepthToViewSpace(depthHistory, surfaceHistoryUV), 1.0));

    vec3 position = ConvertDepthToViewSpace(depth, uv);
    vec3 positionHistory = ConvertDepthToViewSpace(depthHistory, uv);

    float parallax = ComputeParallax(position, positionHistory);
    float NdotV = abs(dot(normalize(-position), normal));

    float dominantFactor = GetSpecularDominantFactor(NdotV, sqrt(roughness));

    vec3 mean, std;
#ifdef UPSCALE
    const int radius = int(mix(3.0, float(kernelRadius), min(1.0, roughness * 4.0)));
#else
    const int radius = int(mix(2.0, float(kernelRadius), min(1.0, roughness * 4.0)));
#endif
    if (roughness < pushConstants.roughnessCutoff) {
        ComputeVarianceMinMax(roughness, radius, mean, std);
    }
    else {
        // Don't need much denoising here (except for roughness = 1. looks noisy for metallic surfaces)
        ComputeVarianceMinMax(roughness, 3, mean, std);
    }

    ivec2 velocityPixel = pixel;

    uv = (vec2(pixel) + vec2(0.5)) * invResolution + velocity;
    vec2 historyPixel = vec2(pixel) + velocity * resolution;
    uv = (historyPixel + 0.5) * invResolution;

    bool valid = true;
    vec4 history = vec4(0.0);
    vec4 historyMoments = vec4(0.0);
    valid = SampleHistory(pixel, historyPixel, 16.0,
        history, historyMoments);

#ifdef BICUBIC_FILTER
    // This should be implemented more efficiently, see 
    if (roughness < pushConstants.roughnessCutoff) {
        vec4 catmullRomHistory;
        bool success = SampleCatmullRom(pixel, uv, catmullRomHistory);
        history = success && valid ? catmullRomHistory : history; 
    }
#endif

    vec3 currentColor = RGBToYCoCg(texelFetch(currentTexture, pixel, 0).rgb);
    
    float rayLength = texelFetch(currentTexture, pixel, 0).a;
    if (rayLength != 0 &&  roughness < pushConstants.roughnessCutoff) {
        vec2 virtualHistoryPixel = VirtualPointReprojection(pixel, imageSize(resolveImage), abs(rayLength));

        bool validRepojection = true;
        vec4 historyVirtualRepojection = vec4(0.0);
        vec4 historyMomentsVirtualRepojection = vec4(0.0);
        validRepojection = SampleHistory(pixel, virtualHistoryPixel, 32.0,
            historyVirtualRepojection, historyMomentsVirtualRepojection);

#ifdef BICUBIC_FILTER
        // This should be implemented more efficiently, see 
        if (validRepojection) {
            vec4 catmullRomHistory;
            uv = (virtualHistoryPixel + 0.5) * invResolution;
            bool success = SampleCatmullRom(pixel, uv, catmullRomHistory);
            historyVirtualRepojection = success ? catmullRomHistory : historyVirtualRepojection; 
        }
#endif

        vec3 virtualNormal = normalize(DecodeNormal(texelFetch(historyNormalTexture, ivec2(virtualHistoryPixel), 0).rg));
        float virtualRoughness = texelFetch(historyRoughnessMetallicAoTexture, ivec2(virtualHistoryPixel), 0).r;

        const float virtualCutoff = 0.2;
        float virtualConfidence = dominantFactor * sqr(max((virtualCutoff - roughness) / virtualCutoff, 0.0));
        virtualConfidence *= pow(max(dot(virtualNormal, normal), 0.0), 512.0);
        virtualConfidence *= pow(1.0 - abs(virtualRoughness - roughness), 32.0);

        valid = valid || validRepojection;
        history = (historyVirtualRepojection * virtualConfidence + 
                (1.0 - virtualConfidence) * history);
    }

    vec2 currentMoments;
    currentMoments.r = currentColor.r;
    currentMoments.g = currentMoments.r * currentMoments.r;

    vec3 historyNeighbourhoodMin = mean - std;
    vec3 historyNeighbourhoodMax = mean + std;

    vec3 currentNeighbourhoodMin = mean - pushConstants.currentClipFactor * std;
    vec3 currentNeighbourhoodMax = mean + pushConstants.currentClipFactor * std;
    currentColor = clamp(currentColor, currentNeighbourhoodMin, currentNeighbourhoodMax);

    vec3 historyColor = RGBToYCoCg(history.rgb);
    float clipBlend = ClipBoundingBox(historyNeighbourhoodMin, historyNeighbourhoodMax,
        historyColor, currentColor);

    // In case of clipping we might also reject the sample. TODO: Investigate
    clipBlend = ClipBoundingBox(historyNeighbourhoodMin, historyNeighbourhoodMax,
        historyColor, currentColor);
    float adjClipBlend = clamp(clipBlend, 0.0, pushConstants.historyClipMax);    

    currentColor = valid ? currentColor : mean;

    historyColor = YCoCgToRGB(historyColor);
    currentColor = YCoCgToRGB(currentColor);

    float temporalWeight = mix(pushConstants.temporalWeight, 0.0, adjClipBlend);

#ifdef UPSCALE
    float roughnessMinTemporalWeight = 0.75;
#else
    float roughnessMinTemporalWeight = temporalWeight;
#endif
    float factor = clamp(32.0 * log(roughness + 1.0), roughnessMinTemporalWeight, temporalWeight);
    //factor = GetAccumulationSpeed(NdotV, roughness, parallax);
    valid = (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0
         || uv.y > 1.0) ? false : valid;

    factor = pushConstants.resetHistory > 0 ? 0.0 : factor;

    float historyLength = historyMoments.b;
    if (factor <= 0.1 * roughness || !valid) {
        historyLength = 0.0;
        currentMoments.g = 1.0;
        currentMoments.r = 0.0;
    }

    factor = max(0.75, factor - 20.0 * max(abs(velocity.x), abs(velocity.y)));
    factor = min(factor, historyLength / (historyLength + 1.0));

#ifdef UPSCALE
    factor = rayLength > 0.0 ? factor : (valid ? mix(1.0, 0.0, adjClipBlend) : factor);
#endif

    vec3 resolve = factor <= 0.0 ? currentColor : mix(currentColor, historyColor, factor);
    vec2 momentsResolve = factor <= 0.0 ? currentMoments : mix(currentMoments, historyMoments.rg, factor);

    // Boost variance when we have a small history length (we trade blur for noise)
    float varianceBoost = max(1.0, 4.0 / (historyLength + 1.0));
    float variance = max(0.0, momentsResolve.g - momentsResolve.r * momentsResolve.r);
    variance *= varianceBoost;

    variance = roughness <= 0.1 ? variance * roughness : variance;

    imageStore(momentsImage, pixel, vec4(momentsResolve, historyLength + 1.0, 0.0));
    imageStore(resolveImage, pixel, vec4(vec3(resolve), variance));

}