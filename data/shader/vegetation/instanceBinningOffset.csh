#include <buffers.hsh>

layout (local_size_x = 64) in;

shared uint counts[64];

void main() {

    uint idx = gl_LocalInvocationIndex;

    counts[idx] = binCounters[idx];

    barrier();

    uint totalOffset = 0;
    for (uint i = 0; i < idx; i++) {
        totalOffset += counts[i];
    }

    // Also want to reset the counters
    binCounters[idx] = 0;
    binOffsets[idx] = totalOffset;

}