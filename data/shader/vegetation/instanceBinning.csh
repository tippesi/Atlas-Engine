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
    if (idx >= instanceCounters[PushConstants.meshIdx])
        return;

    MeshInformation meshInfo = meshInformations[PushConstants.meshIdx];
    uint binOffset = floatBitsToUint(meshInfo.aabbMin.w);

    Instance instance = instanceData[idx];

    uint binIdx = min(GetBin(instance, meshInfo), PushConstants.binCount - 1u) + binOffset;
    uint instanceIdx = atomicAdd(binCounters[binIdx], 1u);

    uint instanceOffset = binOffsets[binIdx];
    instanceDataSwap[instanceIdx + instanceOffset] = instance;

}