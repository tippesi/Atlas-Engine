#define LOCAL_STACK

#include <buffers.hsh>
#include <bvh.hsh>
#include <common.hsh>
#include <tracing.hsh>

layout (local_size_x = 32) in;

const float offset = 0.0;

void main() {
    
    uint index = gl_GlobalInvocationID.x;
    
    Ray ray;
    
    if (index < readAtomic[0]) {
        uint readOffset = (1u - PushConstants.rayBufferOffset) * PushConstants.rayBufferSize;
        ray = UnpackRay(rays[index + readOffset]);

        ray.hitID = -1;
        ray.hitDistance = 0.0;
        if (ray.ID >= 0) {
            // Find any triangle in the BVH
#ifdef OPACITY_CHECK
            HitClosestTransparency(ray, INSTANCE_MASK_ALL, offset, INF);
#else
            HitClosest(ray, INSTANCE_MASK_ALL, offset, INF);
#endif
        }

        uint writeOffset = PushConstants.rayBufferOffset * PushConstants.rayBufferSize;
        rays[index + writeOffset] = PackRay(ray);
    }

}