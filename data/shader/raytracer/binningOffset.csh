#include <buffers.hsh>

layout (local_size_x = 64) in;

shared uint counts[64];

void main() {

    uint idx = gl_LocalInvocationIndex;

    counts[idx] = rayBinCounters[idx];

    barrier();

    uint totalOffset = 0;
    for (uint i = 0; i < idx; i++) {
        totalOffset += counts[i];
    }

    // Also want to reset the counters
    rayBinCounters[idx] = 0;
    rayBinOffsets[idx] = totalOffset;

}