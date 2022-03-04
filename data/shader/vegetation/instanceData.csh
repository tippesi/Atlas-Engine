#include <buffers.hsh>

layout (local_size_x = 64) in;

uniform int instanceCount;
uniform int meshIdx;
uniform vec3 cameraLocation;

uniform vec4 frustumPlanes[6];

bool IsInstanceVisible(Instance instance, MeshInformation meshInfo) {

    vec3 min = meshInfo.aabbMax.xyz + instance.position.xyz;
    vec3 max = meshInfo.aabbMin.xyz + instance.position.xyz;

	for (int i = 0; i < 6; i++) {
		vec3 normal = frustumPlanes[i].xyz;
		float distance = frustumPlanes[i].w;
		
		vec3 s;
		s.x = normal.x >= 0.0 ? min.x : max.x;
		s.y = normal.y >= 0.0 ? min.y : max.y;
		s.z = normal.z >= 0.0 ? min.z : max.z;
				
		if (distance + dot(normal, s) < 0.0) {
			return false;
		}
	}
	return true;
}

void main() {

    int idx = int(gl_GlobalInvocationID.x);
    if (idx >= instanceCount)
        return;

    MeshInformation meshInfo = meshInformations[meshIdx];
    uint counterIdx = floatBitsToUint(meshInfo.aabbMin.w);

    bool cull = false;
    Instance instance = instanceData[idx];

    cull = !IsInstanceVisible(instance, meshInfo) ||
        distance(instance.position.xyz, cameraLocation) > 400.0;

    if (!cull) {
        uint instanceOffset = atomicAdd(lodCounters[counterIdx], 1u);
        culledInstanceData[instanceOffset] = instance;
    }

}