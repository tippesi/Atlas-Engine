#include <buffers.hsh>
#include <bvh.hsh>
#include <common.hsh>

layout (local_size_x = 32) in;

layout (std430, binding = 4) buffer WriteMissRays {
	PackedRay writeMissRays[];
};

void main() {
	
	uint index = gl_GlobalInvocationID.x;
	
	Ray ray;
	
	/*
	if (index < readAtomic[0]) {
		ray = UnpackRay(readRays[index]);

        ray.hitID = -1;
        // Find any triangle in the BVH
        HitAny(ray, 0.0, INF);

        if (ray.hitID >= 0) {
            uint counter = atomicAdd(writeAtomic[0], uint(1));
			writeRays[counter] = PackRay(ray);
        }
	}
	*/

}