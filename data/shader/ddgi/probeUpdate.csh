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

#ifdef IRRADIANCE
layout (local_size_x = 6, local_size_y = 6) in;
#else
#ifdef LOWER_RES_MOMENTS
layout (local_size_x = 6, local_size_y = 6) in;
#else
layout (local_size_x = 14, local_size_y = 14) in;
#endif
#endif

#ifdef IRRADIANCE
layout (set = 3, binding = 0, rgb10_a2) writeonly uniform image2DArray irradiance;
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

const uint sharedSize = 32;

#ifdef IRRADIANCE
struct RayData {
    // This seems to be a better layout than two vec3's
    // My guess is that vec3's get expanded to vec4's
    vec4 direction;
    vec4 radiance;
};
#else
struct RayData {
    vec3 direction;
    float dist;
};
#endif

shared RayData rayData[sharedSize];

void main() {

    uint baseIdx = Flatten3D(ivec3(gl_WorkGroupID.xzy), ivec3(gl_NumWorkGroups.xzy));
    int cascadeIndex = GetProbeCascadeIndex(baseIdx);

    bool reset;
    ivec3 historyProbeCoord;
    GetProbeHistoryInfo(ivec3(gl_WorkGroupID.xyz), cascadeIndex, historyProbeCoord, reset);

    uint historyBaseIdx = Flatten3D(ivec3(historyProbeCoord.xzy), ivec3(gl_NumWorkGroups.xzy));

    uint probeState = floatBitsToUint(historyProbeStates[historyBaseIdx].x);
    float probeAge = historyProbeStates[historyBaseIdx].w;
    vec4 probeOffset = reset ? vec4(0.0, 0.0, 0.0, 1.0) : historyProbeOffsets[historyBaseIdx];

    uint rayBaseIdx = baseIdx * ddgiData.rayCount;
    uint probeRayCount = GetProbeRayCount(probeState);

    uint groupSize = gl_WorkGroupSize.x * gl_WorkGroupSize.y;

#ifdef IRRADIANCE
    ivec2 res = ivec2(ddgiData.volumeIrradianceRes);
#else
    ivec2 res = ivec2(ddgiData.volumeMomentsRes);
#endif

    ivec2 pix = ivec2(gl_LocalInvocationID);
    vec2 coord = (vec2(pix) + vec2(0.5)) / vec2(res);

    vec3 N = OctahedronToUnitVector(coord);

    ivec2 resOffset = (res + ivec2(2)) * ivec2(gl_WorkGroupID.xz) + ivec2(1);
    ivec3 volumeCoord = ivec3(resOffset + pix, int(gl_WorkGroupID.y));

    float cellLength = length(ddgiData.cascades[cascadeIndex].cellSize.xyz);
    float maxDepth = cellLength * 0.75;

    vec4 result = vec4(0.0);
    vec3 newProbeOffset = probeOffset.xyz;
    for (uint i = 0; i < probeRayCount; i += sharedSize) {
        // We might not have a multiple of shared size in terms of rays
        uint loadRayCount = min(sharedSize, probeRayCount - i);

        // Load rays cooperatively
        for (uint j = gl_LocalInvocationIndex; j < loadRayCount; j += groupSize) {
            RayHit hit = UnpackRayHit(hits[rayBaseIdx + i + j]);
            rayData[j].direction.rgb = hit.direction;
#ifdef IRRADIANCE
            rayData[j].radiance.rgb = hit.radiance.rgb;
            rayData[j].direction.a = hit.hitDistance;
#else
            rayData[j].dist = hit.hitDistance;
#endif
        }

        barrier();

        // Iterate over all rays in the shared memory
        for (uint j = 0; j < loadRayCount; j++) {
            
#ifdef IRRADIANCE
            float weight = max(0.0, dot(N, rayData[j].direction.rgb));

            if (weight >= 0.00001) {
                vec3 radiance = vec3(rayData[j].radiance);
                result += vec4(radiance, 1.0) * weight;    
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
                    // newProbeOffset -= rayData[j].direction.xyz * (probeOffsetDistance - dist) * 0.001 * probeOffset.w / probeOffsetDistance;
                }
            }
#else
            float dist = rayData[j].dist;
            dist = dist < 0.0 ? dist * 0.2 : dist;

            float hitDistance = min(maxDepth, dist);
            float weight = max(0.0, dot(N, rayData[j].direction));            

            weight = pow(weight, ddgiData.depthSharpness);
            if (weight >= 0.00000001) {
                result += vec4(hitDistance, sqr(hitDistance), 0.0, 1.0) * weight;
            }
#endif
        }

        barrier();

    }

    ivec2 historyResOffset = (res + ivec2(2)) * ivec2(historyProbeCoord.xz) + ivec2(1);
    ivec3 historyVolumeCoord = ivec3(historyResOffset + pix, int(historyProbeCoord.y));

    // Use a dynamic hysteris based on probe age to accumulate more efficiently at the beginning of a probes life
    float hysteresis = min(ddgiData.hysteresis, probeAge / (probeAge + 1.0));

#ifdef IRRADIANCE
    vec3 lastResult = texelFetch(irradianceVolume, historyVolumeCoord, 0).rgb;
    vec3 resultOut = lastResult;
    if (result.w > 0.0) {
        result.xyz /= result.w;
        result.xyz = pow(result.xyz, vec3(1.0 / ddgiData.volumeGamma));

        if (probeState == PROBE_STATE_NEW || reset) {
            resultOut = result.xyz;
        }
        else {                
            resultOut = mix(result.xyz, lastResult, hysteresis);
        }
    }
    imageStore(irradiance, volumeCoord, vec4(resultOut, 0.0));

    if (gl_LocalInvocationIndex == 0) {
        vec3 maxOffset = ddgiData.cascades[cascadeIndex].cellSize.xyz * 0.5;
        probeOffset.xyz = clamp(newProbeOffset, -maxOffset, maxOffset);
        probeOffset.w = max(0.0, reset ? 1.0 : probeOffset.w - 0.01);
        probeOffsets[baseIdx] = ddgiData.optimizeProbes > 0 ? probeOffset : vec4(0.0, 0.0, 0.0, 1.0);
    }
#else
    vec2 lastResult = texelFetch(momentsVolume, historyVolumeCoord, 0).rg;
    vec2 resultOut = lastResult;
    if (result.w > 0.0) {
        if (probeState == PROBE_STATE_NEW || reset) {
            resultOut = result.xy / result.w;
        }
        else {
            resultOut = mix(result.xy / result.w, lastResult, hysteresis);
        }
    }

    imageStore(moment, volumeCoord, vec4(resultOut, 0.0, 0.0));    
#endif

}