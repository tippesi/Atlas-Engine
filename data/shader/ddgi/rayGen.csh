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

layout (local_size_x = 32) in;

layout(std430, set = 3, binding = 0) buffer RayDirs {
    vec4 rayDirs[];
};

layout(std430, set = 3, binding = 1) buffer RayDirsInactiveProbes {
    vec4 rayDirsInactiveProbes[];
};

layout(std140, set = 3, binding = 2) uniform UniformBuffer {
    mat4 randomRotation;
} Uniforms;

shared uint probeState;
shared vec3 probeOffset;

void main() {

    uint baseIdx = GetProbeIdx(ivec3(gl_WorkGroupID));

    if (gl_LocalInvocationID.x == 0u) {
        probeState = floatBitsToUint(probeStates[baseIdx].x);
        probeOffset = probeOffsets[baseIdx].xyz;
    }

    barrier();

    uint rayBaseIdx = baseIdx * ddgiData.rayCount;
    uint probeRayCount = GetProbeRayCount(probeState);

    uint workGroupOffset = gl_WorkGroupSize.x;
    for(uint i = gl_LocalInvocationIndex; i < probeRayCount; i += workGroupOffset) {
        Ray ray;

        ray.ID = int(rayBaseIdx + i);
        ray.origin = GetProbePosition(ivec3(gl_WorkGroupID)) + probeOffset;
        ray.direction = normalize(mat3(Uniforms.randomRotation) *
            (probeState == PROBE_STATE_INACTIVE ? rayDirsInactiveProbes[i].xyz : rayDirs[i].xyz));

        WriteRay(ray);
    }
}