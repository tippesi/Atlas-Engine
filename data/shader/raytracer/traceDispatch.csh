#include <../common/indirectcommand.hsh>
#include <buffers.hsh>

layout (local_size_x = 1) in;

layout (std430, set = 2, binding = 12) buffer IndirectCommands {
    DispatchIndirectCommand indirectCommands[];
};

void main() {

    // Read atomic counter of previous bounce
    uint rayCount = readAtomic[0];
    
    // Assume a group size of 32 threads
    uint groupCount = uint(ceil(float(rayCount) / 32.0));
    
    DispatchIndirectCommand dispatch;
    
    dispatch.numGroupsX = max(groupCount, uint(1));
    dispatch.numGroupsY = uint(1);
    dispatch.numGroupsZ = uint(1);
    
    indirectCommands[0] = dispatch;
    
    writeAtomic[0] = uint(0);

}