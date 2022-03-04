// Based on DDGI: 
// Majercik, Zander, et al. "Dynamic diffuse global illumination with ray-traced irradiance fields."
// Journal of Computer Graphics Techniques Vol 8.2 (2019).
// Majercik, Zander, et al. "Scaling Probe-Based Real-Time Dynamic Global Illumination for Production."
// arXiv preprint arXiv:2009.10796 (2020).

#include <ddgi.hsh>

#include <../raytracer/structures.hsh>
#include <../raytracer/common.hsh>
#include <../raytracer/buffers.hsh>
#include <../raytracer/tracing.hsh>

#include <../common/random.hsh>
#include <../common/flatten.hsh>
#include <../common/utility.hsh>
#include <../common/barrier.hsh>

layout (local_size_x = 64) in;

layout(std430, binding = 0) buffer RayHits {
	PackedRayHit hits[];
};

layout (binding = 0, rgba16f) writeonly uniform image2DArray irradiance;
layout (binding = 1, rg16f) writeonly uniform image2DArray moment;

uniform int irrProbeRes;
uniform int momProbeRes;
uniform float depthSharpness;

uniform float hysteresis = 0.99;

shared RayHit sharedHits[256];
shared uint increment;
shared uint probeState;

void GetWriteInfo(int probeRes, const bool invert,
    out uint pixelsPerInvocation, out uint localBaseIdx) {
    const uint groupSize = 64;

    uint totalPixelCount = probeRes * probeRes;
    pixelsPerInvocation = totalPixelCount / groupSize;
    uint leftoverPixels = totalPixelCount - groupSize * pixelsPerInvocation;

    uint invocation = invert ? gl_WorkGroupSize.x - gl_LocalInvocationID.x - 1
         : gl_LocalInvocationID.x;
    bool processLeftover = invocation < leftoverPixels;
	pixelsPerInvocation += processLeftover ? 1u : 0u;

    localBaseIdx = invocation * pixelsPerInvocation;
	localBaseIdx += processLeftover ? 0u : leftoverPixels;
}

void UpdateIrradiance() {
    uint pixelsPerInvocation, localBaseIdx;
    GetWriteInfo(irrProbeRes, false, pixelsPerInvocation, localBaseIdx);

    uint probeRayCount = GetProbeRayCount(probeState);

    ivec2 res = ivec2(irrProbeRes, irrProbeRes);
    for (uint j = 0; j < pixelsPerInvocation; j++) {
        uint idx = localBaseIdx + j;
        
        ivec2 pix = Unflatten2D(int(idx), res);
        vec2 coord = (vec2(pix) + vec2(0.5)) / vec2(res);

        vec3 N = OctahedronToUnitVector(coord);

        vec4 result = vec4(0.0);
        int backHits = 0;
        for (uint i = 0; i < probeRayCount; i++) {
            RayHit hit = sharedHits[i];

            float weight = max(0.0, dot(N, hit.direction));

            if (weight >= 0.00001) {
                result += vec4(hit.radiance * weight, weight);	
            }
        }

        ivec2 resOffset = (res + ivec2(2)) * ivec2(gl_WorkGroupID.xz) + ivec2(1);
        ivec3 volumeCoord = ivec3(resOffset + pix, int(gl_WorkGroupID.y));

        vec3 lastResult = texelFetch(irradianceVolume, volumeCoord, 0).rgb;
        vec3 resultOut = lastResult;
        if (result.w > 0.0) {
            result.xyz /= result.w;
            result.xyz = pow(result.xyz, vec3(1.0 / volumeGamma));

            float probeHysteresis = hysteresis;

            // This might need some tweaking, but seems to work reasonably well
            // for a low amount of rays, which means a higher amount of rays should
            // work as well
            if (max3(result.xyz - lastResult) > 0.2) {
                //probeHysteresis = max(0.0, probeHysteresis - 0.01);
            }
            /*
            if (max3(lastResult - result.xyz) > 0.2) {
                probeHysteresis = max(0.0, probeHysteresis - 0.75);
            }

            vec3 delta = result.xyz - lastResult;
            if (length(delta) > 1.0) {
                result.xyz = lastResult + (delta * 0.25);
            }
            */
            if (probeState == PROBE_STATE_NEW) {
                resultOut = result.xyz;
            }
            else {                
                resultOut = mix(result.xyz, lastResult, probeHysteresis);
            }
        }

        imageStore(irradiance, volumeCoord, vec4(resultOut, 0.0));
    }
}

void UpdateMoments() {
    uint pixelsPerInvocation, localBaseIdx;
    GetWriteInfo(momProbeRes, true, pixelsPerInvocation, localBaseIdx);

    uint probeRayCount = GetProbeRayCount(probeState);

    ivec2 res = ivec2(momProbeRes, momProbeRes);
    for (uint j = 0; j < pixelsPerInvocation; j++) {
        uint idx = localBaseIdx + j;
        
        ivec2 pix = Unflatten2D(int(idx), res);
        vec2 coord = (vec2(pix) + vec2(0.5)) / vec2(res);

        vec3 N = OctahedronToUnitVector(coord);

        vec4 result = vec4(0.0);
        for (uint i = 0; i < probeRayCount; i++) {
            RayHit hit = sharedHits[i];

            float dist = hit.hitDistance;
            dist = dist < 0.0 ? dist * 0.2 : dist;

            float hitDistance = min(length(cellSize) * 0.75, dist);
            float weight = max(0.0, dot(N, hit.direction));

            weight = pow(weight, depthSharpness);
            if (weight >= 0.00000001) {
                result += vec4(hitDistance * weight, 
                    sqr(hitDistance) * weight, 0.0, weight);
            }
        }

        ivec2 resOffset = (res + ivec2(2)) * ivec2(gl_WorkGroupID.xz) + ivec2(1);
        ivec3 volumeCoord = ivec3(resOffset + pix, int(gl_WorkGroupID.y));

        vec2 lastResult = texelFetch(momentsVolume, volumeCoord, 0).rg;
        vec2 resultOut = lastResult;
        if (result.w > 0.0) {
            if (probeState == PROBE_STATE_NEW) {
                resultOut = result.xy / result.w;
            }
            else {
                resultOut = mix(result.xy / result.w, lastResult, hysteresis);
            }
        }

        imageStore(moment, volumeCoord, vec4(resultOut, 0.0, 0.0));
    }
}

void main() {

    uint baseIdx = Flatten3D(ivec3(gl_WorkGroupID.xzy), ivec3(gl_NumWorkGroups.xzy));

    if (gl_LocalInvocationID.x == 0u) {
        increment = 0u;
        probeState = probeStates[baseIdx];
    }

    barrier();

    uint rayBaseIdx = baseIdx * rayCount;
    uint probeRayCount = GetProbeRayCount(probeState);

    uint index = atomicAdd(increment, uint(1));
    while(index < probeRayCount) {
        sharedHits[index] = UnpackRayHit(hits[rayBaseIdx + index]);
        index = atomicAdd(increment, uint(1));
    }

    barrier();

    UpdateIrradiance();
    UpdateMoments();

}