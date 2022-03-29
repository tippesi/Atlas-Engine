#include <buffers.hsh>
#include <common.hsh>
#include <tracing.hsh>

layout (local_size_x = 32) in;

void main() {

    if (IsRayInvocationValid()) {
		uint idx = (1u - rayBufferOffset) * rayBufferSize + gl_GlobalInvocationID.x;
        PackedRay packedRay = rays[idx];
        Ray ray = UnpackRay(packedRay);

        uint bin = uint(ray.hitID);
        idx = atomicAdd(rayBinCounters[bin], 1u) + rayBinOffsets[bin];

        uint offset = rayBufferOffset * rayBufferSize;
        rays[offset + idx] = packedRay;
    }

}