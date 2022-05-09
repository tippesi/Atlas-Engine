#include <common.hsh>
#include <buffers.hsh>

layout (local_size_x = 64) in;

uniform int instanceCount;
uniform int meshIdx;
uniform uint binCount;

void main() {

    int idx = int(gl_GlobalInvocationID.x);
    if (idx >= instanceCounters[meshIdx])
        return;

    MeshInformation meshInfo = meshInformations[meshIdx];
    uint binOffset = floatBitsToUint(meshInfo.aabbMin.w);

    Instance instance = instanceData[idx];

    uint binIdx = min(GetBin(instance, meshInfo), binCount - 1u) + binOffset;
    uint instanceIdx = atomicAdd(binCounters[binIdx], 1u);

    uint instanceOffset = binOffsets[binIdx];
    instanceDataSwap[instanceIdx + instanceOffset] = instance;

}