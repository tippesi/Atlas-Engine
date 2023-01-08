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
layout (local_size_x = 14, local_size_y = 14) in;
#endif

layout(std430, set = 3, binding = 1) buffer RayHits {
	PackedRayHit hits[];
};

const uint sharedSize = 64;

#ifdef IRRADIANCE
layout (set = 3, binding = 0, rgb10_a2) writeonly uniform image2DArray irradiance;
#else
layout (set = 3, binding = 0, rg16f) writeonly uniform image2DArray moment;
#endif

#ifdef IRRADIANCE
struct RayData {
    // This seems to be a better layout than two vec3's
    // My guess is that vec3's get expanded to vec4's
    vec4 direction;
    vec2 radiance;
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

    uint probeState = floatBitsToUint(probeStates[baseIdx].x);
    vec4 probeOffset = probeOffsets[baseIdx];

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

    float cellLength = length(ddgiData.cellSize.xyz);
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
            rayData[j].radiance.rg = hit.radiance.rg;
            rayData[j].direction.a = hit.radiance.b;
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
                vec3 radiance = vec3(rayData[j].radiance, rayData[j].direction.a);
                result += vec4(radiance, 1.0) * weight;	
            }
#else
            float dist = rayData[j].dist;
            dist = dist < 0.0 ? dist * 0.2 : dist;

            float hitDistance = min(maxDepth, dist);
            float weight = max(0.0, dot(N, rayData[j].direction));

            const float probeOffsetDistance = 0.6;
            dist = rayData[j].dist;
            // Remember: Negative distances means backface hits.
            // Meaning we want to get probes from backfaces to the 
            // front and want to get a certain distance to these front
            // faces to gather more useful information per probe.
            // Each probe is dampend by its own factor, which is
            // reduced in each frame to stop them from moving indefinitely
            if (probeOffset.w > 0.0) {
                float sig = sign(dist);
                if (abs(dist) < probeOffsetDistance && ddgiData.optimizeProbes > 0) {
                    newProbeOffset -= rayData[j].direction * (sig * probeOffsetDistance - dist) * 0.1 * probeOffset.w;
                }
            }

            weight = pow(weight, ddgiData.depthSharpness);
            if (weight >= 0.00000001) {
                result += vec4(hitDistance, sqr(hitDistance), 0.0, 1.0) * weight;
            }
#endif
        }

        barrier();

    }

#ifdef IRRADIANCE
    vec3 lastResult = texelFetch(irradianceVolume, volumeCoord, 0).rgb;
    vec3 resultOut = lastResult;
    if (result.w > 0.0) {
        result.xyz /= result.w;
        result.xyz = pow(result.xyz, vec3(1.0 / ddgiData.volumeGamma));

        float probeHysteresis = ddgiData.hysteresis;

        if (probeState == PROBE_STATE_NEW) {
            resultOut = result.xyz;
        }
        else {                
            resultOut = mix(result.xyz, lastResult, probeHysteresis);
        }
    }
    imageStore(irradiance, volumeCoord, vec4(resultOut, 0.0));
#else
    vec2 lastResult = texelFetch(momentsVolume, volumeCoord, 0).rg;
    vec2 resultOut = lastResult;
    if (result.w > 0.0) {
        if (probeState == PROBE_STATE_NEW) {
            resultOut = result.xy / result.w;
        }
        else {
            resultOut = mix(result.xy / result.w, lastResult, ddgiData.hysteresis);
        }
    }

    imageStore(moment, volumeCoord, vec4(resultOut, 0.0, 0.0));
    if (gl_LocalInvocationIndex == 0 && ddgiData.optimizeProbes > 0) {
        vec3 maxOffset = ddgiData.cellSize.xyz * 0.5;
        probeOffset.xyz = clamp(newProbeOffset, -maxOffset, maxOffset);
        probeOffset.w = max(0.0, probeOffset.w - 0.01);
        probeOffsets[baseIdx] = probeOffset;
    }
#endif

}