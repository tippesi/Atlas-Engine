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

layout(std430, binding = 0) buffer RayHits {
	PackedRayHit hits[];
};

layout(std430, binding = 1) buffer ProbeStateTemporal {
	vec4 temporalInfos[];
};

uniform float cellLength;

shared uint increment;
shared uint probeState;
shared uint backFaceHits;
shared uint inCellHits;
shared vec4 temporalInfo;

void main() {

    uint baseIdx = Flatten3D(ivec3(gl_WorkGroupID.xzy), ivec3(gl_NumWorkGroups.xzy));
    uint rayBaseIdx = baseIdx * rayCount;

	if (gl_LocalInvocationID.x == 0u) {
        increment = 0u;
        backFaceHits = 0u;
        inCellHits = 0u;
        probeState = floatBitsToUint(probeStates[baseIdx].w);
        temporalInfo = temporalInfos[baseIdx];
    }

    barrier();

    uint probeRayCount = GetProbeRayCount(probeState);
    // Use an extended size to avoid potential flickering
    // due to not having full sampling of the environment
    float extendedSize = cellLength * 1.0;

    uint idx = atomicAdd(increment, uint(1));
    while(idx < probeRayCount) {
		RayHit hit = UnpackRayHit(hits[rayBaseIdx + idx]);

        bool backface = hit.hitDistance <= 0.0;
        if (backface) {
            atomicAdd(backFaceHits, uint(1));
        }
        if (hit.hitDistance < extendedSize && !backface) {
            atomicAdd(inCellHits, uint(1));
        }
        idx = atomicAdd(increment, uint(1));
    }

    barrier();

    if (gl_LocalInvocationID.x == 0u) {
        probeState = PROBE_STATE_INACTIVE;
        float percentageBackface = float(backFaceHits) / float(probeRayCount);
        temporalInfo.x = max(0.0, temporalInfo.x - 1.0);
        if (inCellHits > 0u && percentageBackface < 0.2) {
            // Keep the state for 20 frames
            temporalInfo.x = 20.0;
        }
        if (temporalInfo.x > 0.0) {
            probeState = PROBE_STATE_ACTIVE;
        }
        probeStates[baseIdx].w = uintBitsToFloat(probeState);
        temporalInfos[baseIdx] = temporalInfo;
    }

}