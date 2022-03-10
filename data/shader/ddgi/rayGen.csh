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

uniform mat3 randomRotation;

layout(std430, binding = 4) buffer RayDirs {
	vec4 rayDirs[];
};

layout(std430, binding = 5) buffer RayDirsInactiveProbes {
	vec4 rayDirsInactiveProbes[];
};

shared uint increment;
shared uint probeState;
shared vec3 probeOffset;

void main() {

    uint baseIdx = GetProbeIdx(ivec3(gl_WorkGroupID));

	if (gl_LocalInvocationID.x == 0u) {
        increment = 0u;
        probeState = floatBitsToUint(probeStates[baseIdx].w);
        probeOffset = probeStates[baseIdx].xyz;
    }

    barrier();

    uint rayBaseIdx = baseIdx * rayCount;
    uint probeRayCount = GetProbeRayCount(probeState);

    uint idx = atomicAdd(increment, uint(1));
    while(idx < probeRayCount) {
		Ray ray;

        ray.ID = int(rayBaseIdx + idx);
		ray.origin = GetProbePosition(ivec3(gl_WorkGroupID)) + probeOffset;
		ray.direction = normalize(randomRotation * 
            (probeState == PROBE_STATE_INACTIVE ? rayDirsInactiveProbes[idx].xyz : rayDirs[idx].xyz));

		WriteRay(ray);
        idx = atomicAdd(increment, uint(1));
    }
}