#include <buffers.hsh>

layout (local_size_x = 64) in;

layout(push_constant) uniform constants {
    int drawCallCount;
} PushConstants;

void main() {

    if (int(gl_GlobalInvocationID.x) >= PushConstants.drawCallCount)
        return;

    uint meshSubdataIdx = gl_GlobalInvocationID.x / 64u;
    MeshSubdataInformation subDataInfo = meshSubdataInformations[meshSubdataIdx];
    MeshInformation meshInfo = meshInformations[subDataInfo.meshIdx];

    uint binOffset = floatBitsToUint(meshInfo.aabbMin.w);
    uint binIdx = gl_GlobalInvocationID.x % 64u;

    uint instanceCount = binCounters[binOffset + binIdx];
    uint instanceOffset = binOffsets[binOffset + binIdx];

    DrawElementsIndirectCommand command;

    bool hasInstances = instanceCount > 0;

    // We assume triangles
    command.count = hasInstances ? subDataInfo.indicesCount : 0;
    command.instanceCount = instanceCount;
    command.firstIndex = 0;
    command.baseVertex = 0;
    command.baseInstance = hasInstances ? instanceOffset : 0;

    indirectCommands[gl_GlobalInvocationID.x] = command;

}