#include <buffers.hsh>

layout (local_size_x = 64) in;

uniform int drawCallCount;

void main() {

    if (int(gl_GlobalInvocationID.x) >= drawCallCount)
        return;

    MeshSubdataInformation subDataInfo = meshSubdataInformations[gl_GlobalInvocationID.x];
    MeshInformation meshInfo = meshInformations[subDataInfo.meshIdx];

    uint counterIdx = floatBitsToUint(meshInfo.aabbMin.w);
    uint instanceCount = lodCounters[counterIdx];

    DrawElementsIndirectCommand command;

    // We assume triangles
    command.count = 3 * subDataInfo.indicesCount;
    command.instanceCount = instanceCount;
    command.firstIndex = 0;
    command.baseVertex = 0;
    command.baseInstance = 0;

    indirectCommands[gl_GlobalInvocationID.x] = command;

}