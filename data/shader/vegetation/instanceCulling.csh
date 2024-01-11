#include <../globals.hsh>
#include <common.hsh>
#include <buffers.hsh>

layout (local_size_x = 64) in;

layout(push_constant) uniform constants {
    int instanceCount;
    int meshIdx;
    uint binCount;
} PushConstants;

void main() {

    int idx = int(gl_GlobalInvocationID.x);
    if (idx >= PushConstants.instanceCount)
        return;

    MeshInformation meshInfo = meshInformations[PushConstants.meshIdx];
    uint binOffset = floatBitsToUint(meshInfo.aabbMin.w);

    bool cull = false;
    Instance instance = instanceData[idx];

    float dist = distance(instance.position.xyz, globalData[0].cameraLocation.xyz);
    uint binIdx = min(GetBin(instance, meshInfo), PushConstants.binCount - 1u) + binOffset;

    cull = !IsInstanceVisible(instance, meshInfo) || dist > 100.0;
    //cull = dist > 25 ? idx % 2 == 0 ? cull : true : cull;
    //cull = dist > 50 ? idx % 4 == 0 ? cull : true : cull;

    if (!cull) {
        uint instanceOffset = atomicAdd(instanceCounters[PushConstants.meshIdx], 1u);
        instanceDataSwap[instanceOffset] = instance;
        atomicAdd(binCounters[binIdx], 1u);
    }

}