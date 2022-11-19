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

uniform float cellLength;

shared uint probeState;
shared uint backFaceHits;
shared uint inCellHits;
shared float temporalPercentageBackface;

void main() {

    uint baseIdx = Flatten3D(ivec3(gl_WorkGroupID.xzy), ivec3(gl_NumWorkGroups.xzy));
    uint rayBaseIdx = baseIdx * rayCount;

	if (gl_LocalInvocationID.x == 0u) {
        backFaceHits = 0u;
        inCellHits = 0u;
        probeState = GetProbeState(baseIdx);
        temporalPercentageBackface = probeStates[baseIdx].y;
    }

    barrier();

    uint probeRayCount = GetProbeRayCount(probeState);
    // Use an extended size to avoid potential flickering
    // due to not having full sampling of the environment
    float extendedSize = cellLength * 1.0;

    uint workGroupOffset = gl_WorkGroupSize.x;
    for(uint i = gl_LocalInvocationIndex; i < probeRayCount; i += workGroupOffset) {
		RayHit hit = UnpackRayHit(hits[rayBaseIdx + i]);

        bool backface = hit.hitDistance <= 0.0;
        if (backface) {
            atomicAdd(backFaceHits, uint(1));
        }
        if (hit.hitDistance < extendedSize && !backface) {
            atomicAdd(inCellHits, uint(1));
        }
    }

    barrier();

    if (gl_LocalInvocationID.x == 0u) {
        probeState = PROBE_STATE_INACTIVE;

        float percentageBackface = float(backFaceHits) / float(probeRayCount);
        temporalPercentageBackface = mix(percentageBackface, temporalPercentageBackface, hysteresis);
        // Use temporally stable information to decide probe state
        if (inCellHits > 0u && temporalPercentageBackface < 0.2) {
            probeState = PROBE_STATE_ACTIVE;
        }

        //probeState = PROBE_STATE_ACTIVE;
        
        probeStates[baseIdx].x = uintBitsToFloat(probeState);
        probeStates[baseIdx].y = temporalPercentageBackface;
    }

}