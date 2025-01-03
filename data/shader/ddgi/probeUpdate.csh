// Based on DDGI: 
// Majercik, Zander, et al. "Dynamic diffuse global illumination with ray-traced irradiance fields."
// Journal of Computer Graphics Techniques Vol 8.2 (2019).
// Majercik, Zander, et al. "Scaling Probe-Based Real-Time Dynamic Global Illumination for Production."
// arXiv preprint arXiv:2009.10796 (2020).
// Inspired partially by https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/shaders/ddgi_updateCS.hlsl

#include <ddgi.hsh>

#include <../raytracer/structures.hsh>
#include <../raytracer/common.hsh>
#include <../raytracer/buffers.hsh>
#include <../raytracer/tracing.hsh>

#include <../common/random.hsh>
#include <../common/flatten.hsh>
#include <../common/utility.hsh>
#include <../common/barrier.hsh>
#include <../common/types.hsh>

#if defined(IRRADIANCE)
layout (local_size_x = 6, local_size_y = 6) in;
#elif defined(RADIANCE)
#ifdef LOWER_RES_RADIANCE
layout (local_size_x = 14, local_size_y = 14) in;
#else
layout (local_size_x = 30, local_size_y = 30) in;
#endif
#else
#ifdef LOWER_RES_MOMENTS
layout (local_size_x = 6, local_size_y = 6) in;
#else
layout (local_size_x = 14, local_size_y = 14) in;
#endif
#endif

#if defined(IRRADIANCE)
layout (set = 3, binding = 0, rgb10_a2) writeonly uniform image2DArray irradiance;
#elif defined(RADIANCE)
layout (set = 3, binding = 0, rgb10_a2) writeonly uniform image2DArray radiance;
#else
layout (set = 3, binding = 0, rg16f) writeonly uniform image2DArray moment;
#endif

layout(std430, set = 3, binding = 1) buffer RayHits {
    PackedRayHit hits[];
};

layout(std430, set = 3, binding = 2) buffer HistoryProbeStates {
    vec4 historyProbeStates[];
};

layout(std430, set = 3, binding = 3) buffer HistoryProbeOffsets {
    vec4 historyProbeOffsets[];
};

const uint sharedSize = 128;

#if defined(IRRADIANCE) || defined(RADIANCE)
struct RayData {
    // This seems to be a better layout than two vec3's
    // My guess is that vec3's get expanded to vec4's
#ifndef AE_HALF_FLOAT
    vec4 direction;
    vec4 radiance;
#else
    AeF16x4 direction;
    AeF16x4 radiance;
#endif
};
#else
struct RayData {
    AeF16x4 direction;
};
#endif

shared RayData rayData[sharedSize];

vec4 GetHistoryFromHigherCascade(ivec2 pixel, int cascadeIndex) {    

    ivec3 workGroup = ivec3(gl_WorkGroupID.xyz);
    workGroup.y %= ddgiData.volumeProbeCount.y;

    vec3 probePosition = vec3(workGroup) * ddgiData.cascades[cascadeIndex].cellSize.xyz + ddgiData.cascades[cascadeIndex].volumeMin.xyz;

    // Note: Cascades are orderd from biggest (index 0) to smallest (index cascadeCount - 1)
    cascadeIndex = max(cascadeIndex - 1, 0);

    vec3 localPosition = probePosition - ddgiData.cascades[cascadeIndex].volumeMin.xyz;
    ivec3 cascadeProbeOffset = ivec3(0, cascadeIndex * ddgiData.volumeProbeCount.y, 0);

    ivec3 baseCell = ivec3(localPosition / ddgiData.cascades[cascadeIndex].cellSize.xyz) + cascadeProbeOffset;

    bool reset;
    ivec3 historyProbeCoord;
    GetProbeHistoryInfo(baseCell, cascadeIndex, historyProbeCoord, reset);

#if defined(IRRADIANCE)
    ivec2 res = ivec2(ddgiData.volumeIrradianceRes);
#elif defined(RADIANCE)
    ivec2 res = ivec2(ddgiData.volumeRadianceRes);
#else
    ivec2 res = ivec2(ddgiData.volumeMomentsRes);
#endif

    ivec2 historyResOffset = (res + ivec2(2)) * ivec2(historyProbeCoord.xz) + ivec2(1);
    ivec3 historyVolumeCoord = ivec3(historyResOffset + pixel, int(historyProbeCoord.y));

#if defined(IRRADIANCE)
    vec3 lastResult = texelFetch(irradianceVolume, historyVolumeCoord, 0).rgb;
    return vec4(lastResult, 0.0); 
#elif defined(RADIANCE)
    vec3 lastResult = texelFetch(radianceVolume, historyVolumeCoord, 0).rgb;
    return vec4(lastResult, 0.0); 
#else
    vec2 lastResult = texelFetch(momentsVolume, historyVolumeCoord, 0).rg;
    return vec4(lastResult, 0.0, 0.0);
#endif

}

void main() {

    uint baseIdx = Flatten3D(ivec3(gl_WorkGroupID.xzy), ivec3(gl_NumWorkGroups.xzy));
    int cascadeIndex = GetProbeCascadeIndex(baseIdx);

    bool reset;
    ivec3 historyProbeCoord;
    GetProbeHistoryInfo(ivec3(gl_WorkGroupID.xyz), cascadeIndex, historyProbeCoord, reset);

    uint historyBaseIdx = Flatten3D(ivec3(historyProbeCoord.xzy), ivec3(gl_NumWorkGroups.xzy));

    uint probeState = floatBitsToUint(historyProbeStates[historyBaseIdx].x);
    float probeAge = probeStates[baseIdx].w;
    vec4 probeOffset = reset ? vec4(0.0, 0.0, 0.0, 1.0) : historyProbeOffsets[historyBaseIdx];

    uint rayBaseIdx = baseIdx * ddgiData.rayCount;
    uint probeRayCount = GetProbeRayCount(probeState);

    uint groupSize = gl_WorkGroupSize.x * gl_WorkGroupSize.y;

#if defined(IRRADIANCE)
    ivec2 res = ivec2(ddgiData.volumeIrradianceRes);
#elif defined(RADIANCE)
    ivec2 res = ivec2(ddgiData.volumeRadianceRes);
#else
    ivec2 res = ivec2(ddgiData.volumeMomentsRes);
#endif

    ivec2 pix = ivec2(gl_LocalInvocationID);
    vec2 coord = (vec2(pix) + vec2(0.5)) / vec2(res);

    AeF16x3 N = AeF16x3(OctahedronToUnitVector(coord));

    ivec2 resOffset = (res + ivec2(2)) * ivec2(gl_WorkGroupID.xz) + ivec2(1);
    ivec3 volumeCoord = ivec3(resOffset + pix, int(gl_WorkGroupID.y));

    AeF16 cellLength = AeF16(length(ddgiData.cascades[cascadeIndex].cellSize.xyz));
    AeF16 maxDepth = cellLength * AeF16(0.75);

    AeF16 depthSharpness = AeF16(ddgiData.depthSharpness);

    AeF16x4 result = AeF16x4(0.0);
    vec3 newProbeOffset = probeOffset.xyz;
    for (uint i = 0; i < probeRayCount; i += sharedSize) {
        // We might not have a multiple of shared size in terms of rays
        uint loadRayCount = min(sharedSize, probeRayCount - i);

        // Load rays cooperatively
        for (uint j = gl_LocalInvocationIndex; j < loadRayCount; j += groupSize) {
            RayHit hit = UnpackRayHit(hits[rayBaseIdx + i + j]);
            rayData[j].direction.rgb = hit.direction.xyz;
#if defined(IRRADIANCE) || defined(RADIANCE)
            rayData[j].radiance.rgb = hit.radiance.rgb;
            rayData[j].direction.a = hit.radiance.a;
#else
#ifndef AE_HALF_FLOAT
            float dist = hit.radiance.a;
            dist = dist < 0.0 ? dist * 0.2 : dist;

            float hitDistance = min(maxDepth, dist);
#else
            AeF16 dist = hit.radiance.a;
            dist = dist < AeF16(0.0) ? dist * AeF16(0.2) : dist;

            AeF16 hitDistance = min(maxDepth, dist);
#endif
            rayData[j].direction.w = hitDistance;
#endif
        }

        barrier();

        float maxWeight = 0.0;

        // Iterate over all rays in the shared memory
        for (uint j = 0; j < loadRayCount; j++) {
            
#if defined(IRRADIANCE)
            AeF16 weight = max(AeF16(0.0), dot(N, rayData[j].direction.rgb));

            if (weight >= AeF16(0.00001)) {
                AeF16x3 radiance = AeF16x3(rayData[j].radiance);
                result += AeF16x4(radiance, AeF16(1.0)) * weight;    
            }

            const float probeOffsetDistance = max(ddgiData.cascades[cascadeIndex].cellSize.w * 0.05, 0.5);
            // Remember: Negative distances means backface hits.
            // Meaning we want to get probes from backfaces to the 
            // front and want to get a certain distance to these front
            // faces to gather more useful information per probe.
            // Each probe is dampend by its own factor, which is
            // reduced in each frame to stop them from moving indefinitely
            float dist = rayData[j].direction.a;
            if (probeOffset.w > 0.0) {
                float sig = sign(dist);
                if (dist < 0.0 && -dist < probeOffsetDistance && ddgiData.optimizeProbes > 0) {
                    newProbeOffset -= rayData[j].direction.xyz * (sig * probeOffsetDistance - dist) * 0.02 * probeOffset.w / probeOffsetDistance;
                }

                // This might be used to move probes closer to geometry and make the visibility test more effective, disabled for now
                if (dist > 0.0 && dist < probeOffsetDistance && ddgiData.optimizeProbes > 0) {
                    //newProbeOffset -= rayData[j].direction.xyz * (probeOffsetDistance - dist) * 0.001 * probeOffset.w / probeOffsetDistance;
                }
            }
#elif defined(RADIANCE)            
            AeF16 weight = max(AeF16(0.0), dot(N, rayData[j].direction.rgb));
            weight = pow(weight, AeF16(256.0));            

            AeF16x3 radiance = rayData[j].radiance.xyz;

            if (weight > 0.15)
                result += AeF16x4(radiance, AeF16(1.0)) * weight;
#else
            AeF16 weight = max(AeF16(0.0), dot(N, rayData[j].direction.xyz));

            AeF16 hitDistance = rayData[j].direction.w;

            weight = pow(weight, depthSharpness);
            if (weight >= AeF16(0.00000001)) {
                result += AeF16x4(hitDistance, sqr(hitDistance), AeF16(0.0), AeF16(1.0)) * weight;
            }
#endif
        }

        barrier();

    }

    ivec2 historyResOffset = (res + ivec2(2)) * ivec2(historyProbeCoord.xz) + ivec2(1);
    ivec3 historyVolumeCoord = ivec3(historyResOffset + pix, int(historyProbeCoord.y));

    // Use a dynamic hysteris based on probe age to accumulate more efficiently at the beginning of a probes life
    float hysteresis = clamp(probeAge / (probeAge + 1.0), 0.9, ddgiData.hysteresis);
    hysteresis = ddgiData.hysteresis;

#if defined(IRRADIANCE)
    vec3 lastResult = texelFetch(irradianceVolume, historyVolumeCoord, 0).rgb;
    if (probeState == PROBE_STATE_NEW || reset) {
        lastResult = GetHistoryFromHigherCascade(pix, cascadeIndex).rgb;
    }
    vec3 resultOut = lastResult;
    if (result.w > 0.0) {
        vec4 fullResult = vec4(result);
        fullResult.xyz /= fullResult.w;
        fullResult.xyz = pow(fullResult.xyz, vec3(1.0 / ddgiData.volumeGamma));

        if (probeState == PROBE_STATE_NEW || reset) {
            resultOut = mix(fullResult.xyz, lastResult, hysteresis);
        }
        else {                
            resultOut = mix(fullResult.xyz, lastResult, hysteresis);
        }
    }
    imageStore(irradiance, volumeCoord, vec4(resultOut, 0.0));

    if (gl_LocalInvocationIndex == 0) {
        vec3 maxOffset = ddgiData.cascades[cascadeIndex].cellSize.xyz * 0.5;
        probeOffset.xyz = clamp(newProbeOffset, -maxOffset, maxOffset);
        probeOffset.w = max(0.0, reset ? 1.0 : probeOffset.w - 0.01);
        probeOffsets[baseIdx] = ddgiData.optimizeProbes > 0 ? probeOffset : vec4(0.0, 0.0, 0.0, 1.0);
    }
#elif defined(RADIANCE)
    vec3 lastResult = texelFetch(radianceVolume, historyVolumeCoord, 0).rgb;
    if (probeState == PROBE_STATE_NEW || reset) {
        lastResult = GetHistoryFromHigherCascade(pix, cascadeIndex).rgb;
    }
    vec3 resultOut = lastResult;
    if (result.w > 0.0) {
        vec4 fullResult = vec4(result);
        fullResult.xyz /= fullResult.w;
        fullResult.xyz = pow(fullResult.xyz, vec3(1.0 / ddgiData.volumeGamma));

        if (probeState == PROBE_STATE_NEW || reset) {
            resultOut = mix(fullResult.xyz, lastResult, hysteresis);
        }
        else {                
            resultOut = mix(fullResult.xyz, lastResult, hysteresis);
        }
    }
    imageStore(radiance, volumeCoord, vec4(resultOut, 0.0));
#else
    vec2 lastResult = texelFetch(momentsVolume, historyVolumeCoord, 0).rg;
    if (probeState == PROBE_STATE_NEW || reset) {
        lastResult = GetHistoryFromHigherCascade(pix, cascadeIndex).rg;
    }
    vec2 resultOut = lastResult;
    if (result.w > 0.0) {
        vec4 fullResult = vec4(result);
        if (probeState == PROBE_STATE_NEW || reset) {
            resultOut = fullResult.xy / fullResult.w;
        }
        else {
            resultOut = mix(fullResult.xy / fullResult.w, lastResult, hysteresis);
        }
    }

    imageStore(moment, volumeCoord, vec4(resultOut, 0.0, 0.0));    
#endif

}