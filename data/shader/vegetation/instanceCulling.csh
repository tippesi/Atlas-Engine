#include <common.hsh>
#include <buffers.hsh>

layout (local_size_x = 64) in;

uniform int instanceCount;
uniform int meshIdx;
uniform uint binCount;

void main() {

    int idx = int(gl_GlobalInvocationID.x);
    if (idx >= instanceCount)
        return;

    MeshInformation meshInfo = meshInformations[meshIdx];
    uint binOffset = floatBitsToUint(meshInfo.aabbMin.w);

    bool cull = false;
    Instance instance = instanceData[idx];

    float dist = distance(instance.position.xyz, cameraLocation);
    uint binIdx = min(GetBin(instance, meshInfo), binCount - 1u) + binOffset;

    cull = !IsInstanceVisible(instance, meshInfo) || dist > 100.0;
    //cull = dist > 25 ? idx % 2 == 0 ? cull : true : cull;
    //cull = dist > 50 ? idx % 4 == 0 ? cull : true : cull;

    if (!cull) {
        uint instanceOffset = atomicAdd(instanceCounters[meshIdx], 1u);
        instanceDataSwap[instanceOffset] = instance;
        atomicAdd(binCounters[binIdx], 1u);
    }

}