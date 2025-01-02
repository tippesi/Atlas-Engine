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

layout(std430, set = 3, binding = 2) buffer HistoryProbeStates {
    vec4 historyProbeStates[];
};

layout(std430, set = 3, binding = 3) buffer HistoryProbeOffsets {
    vec4 historyProbeOffsets[];
};

layout(std140, set = 3, binding = 4) uniform UniformBuffer {
    mat4 randomRotation;
} Uniforms;

shared uint probeState;
shared vec3 probeOffset;

bool IsProbeVisible(vec3 probePosition, int cascadeIndex) {

    vec3 cellSize = ddgiData.cascades[cascadeIndex].cellSize.xyz;

    vec3 min = probePosition - cellSize;
    vec3 max = probePosition + cellSize;

    for (int i = 0; i < 6; i++) {
        vec3 normal = globalData.frustumPlanes[i].xyz;
        float dist = globalData.frustumPlanes[i].w;
        
        vec3 s;
        s.x = normal.x >= 0.0 ? max.x : min.x;
        s.y = normal.y >= 0.0 ? max.y : min.y;
        s.z = normal.z >= 0.0 ? max.z : min.z;
                
        if (dist + dot(normal, s) < 0.0) {
            return false;
        }
    }
    return true;
}

void main() {

    ivec3 probeIndex = ivec3(gl_WorkGroupID);
    int cascadeIndex = GetProbeCascadeIndex(probeIndex);

    probeIndex.y %= ddgiData.volumeProbeCount.y;
    uint baseIdx = GetProbeIdx(probeIndex, cascadeIndex);

    bool reset;
    ivec3 historyProbeCoord;
    GetProbeHistoryInfo(ivec3(gl_WorkGroupID.xyz), cascadeIndex, historyProbeCoord, reset);

    uint historyBaseIdx = Flatten3D(ivec3(historyProbeCoord.xzy), ivec3(gl_NumWorkGroups.xzy));

    if (gl_LocalInvocationIndex == 0u) {
        probeState = floatBitsToUint(historyProbeStates[historyBaseIdx].x);
        probeOffset = probeState == PROBE_STATE_NEW || reset ? vec3(0.0) :vec3(0.0);
    }

    barrier();

    vec3 probePosition = GetProbePosition(probeIndex, cascadeIndex);
    if (!IsProbeVisible(probePosition, cascadeIndex)) {
        probeState = PROBE_STATE_INVISIBLE;
        if (gl_LocalInvocationIndex == 0u) {
            historyProbeStates[historyBaseIdx].x = uintBitsToFloat(PROBE_STATE_INVISIBLE);
        }
    }

    uint rayBaseIdx = baseIdx * ddgiData.rayCount;
    uint probeRayCount = GetProbeRayCount(probeState);

    // We could atomic increment the global ray counter by the probeRayCount once an the just write the data
    uint workGroupOffset = gl_WorkGroupSize.x;
    for(uint i = gl_LocalInvocationIndex; i < probeRayCount; i += workGroupOffset) {
        Ray ray;
        if (i < probeRayCount) {       
            ray.ID = int(rayBaseIdx + i);
            ray.origin = probePosition + probeOffset;
            ray.direction = normalize(mat3(Uniforms.randomRotation) *
                (probeState == PROBE_STATE_INACTIVE || probeState == PROBE_STATE_INVISIBLE ? rayDirsInactiveProbes[i].xyz : rayDirs[i].xyz));
        }
        else {
            ray.ID = -1;
        }

        WriteRay(ray);
    }
}