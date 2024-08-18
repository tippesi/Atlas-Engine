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

layout(std430, set = 3, binding = 1) buffer RayHits {
    PackedRayHit hits[];
};

layout(std430, set = 3, binding = 2) buffer HistoryProbeStates {
    vec4 historyProbeStates[];
};

layout(std430, set = 3, binding = 3) buffer HistoryProbeOffsets {
    vec4 historyProbeOffsets[];
};

shared uint probeState;
shared uint backFaceHits;
shared uint inCellHits;
shared float temporalCellHits;
shared float temporalBackFaceRatio;

void main() {

    uint baseIdx = Flatten3D(ivec3(gl_WorkGroupID.xzy), ivec3(gl_NumWorkGroups.xzy));
    uint rayBaseIdx = baseIdx * ddgiData.rayCount;

    int cascadeIndex = GetProbeCascadeIndex(baseIdx);

    bool reset;
    ivec3 historyProbeCoord;
    GetProbeHistoryInfo(ivec3(gl_WorkGroupID.xyz), cascadeIndex, historyProbeCoord, reset);

    uint historyBaseIdx = Flatten3D(ivec3(historyProbeCoord.xzy), ivec3(gl_NumWorkGroups.xzy));

    if (gl_LocalInvocationID.x == 0u) {
        backFaceHits = 0u;
        inCellHits = 0u;
        probeState = GetProbeState(historyBaseIdx);
        temporalCellHits = (probeState == PROBE_STATE_NEW || reset) ? 1.0 : historyProbeStates[historyBaseIdx].y;
        temporalBackFaceRatio = (probeState == PROBE_STATE_NEW || reset) ? 0.25 : historyProbeStates[historyBaseIdx].z;
    }

    barrier();

    uint probeRayCount = GetProbeRayCount(probeState);

    // Use an extended size to avoid potential flickering
    // due to not having full sampling of the environment
    float extendedSize = max3(ddgiData.cascades[cascadeIndex].cellSize.xyz) * 1.0;

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
        uint historyProbeState = probeState;
        probeState = PROBE_STATE_INACTIVE;

        temporalCellHits = mix(float(inCellHits), temporalCellHits, ddgiData.hysteresis);

        // Use temporally stable information to decide probe state
        if (temporalCellHits > 0.01) {
            probeState = PROBE_STATE_ACTIVE;
        }

        float backFaceRatio = float(backFaceHits) / probeRayCount;
        temporalBackFaceRatio = mix(float(backFaceRatio), temporalBackFaceRatio, ddgiData.hysteresis);

        if (temporalBackFaceRatio > 0.25) {
            probeState = PROBE_STATE_INACTIVE;
        }

        probeStates[baseIdx].x = uintBitsToFloat(probeState);
        probeStates[baseIdx].y = temporalCellHits;
        probeStates[baseIdx].z = temporalBackFaceRatio;
        probeStates[baseIdx].w = (historyProbeState == PROBE_STATE_NEW || reset) ? 1.0 : min(256.0, historyProbeStates[historyBaseIdx].w + 1.0);
    }

}