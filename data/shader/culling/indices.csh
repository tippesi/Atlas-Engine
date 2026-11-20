#include <common.hsh>

// Writes the indices of the visible instances at the offsets of their draws, each offset ends up behind its draw
// OCCLUSION writes the instances the occlusion test counted

layout (local_size_x = 64) in;

layout(std430, set = 3, binding = 0) readonly buffer Instances {
    Instance instances[];
};

layout(std430, set = 3, binding = 1) readonly buffer Meshes {
    CullMesh meshes[];
};

layout(std430, set = 3, binding = 2) readonly buffer Views {
    CullView views[];
};

layout(std430, set = 3, binding = 3) readonly buffer Results {
    uvec2 results[];
};

layout(std430, set = 3, binding = 4) buffer Offsets {
    uint offsets[];
};

layout(std430, set = 3, binding = 5) writeonly buffer Indices {
    uint indices[];
};

#ifdef OCCLUSION
layout(std430, set = 3, binding = 6) readonly buffer OcclusionInstances {
    uint occlusionGroupCountX;
    uint occlusionGroupCountY;
    uint occlusionGroupCountZ;
    uint occlusionInstanceCount;
    uint occlusionInstances[];
};
#endif

layout(push_constant) uniform constants {
    uint firstView;
    uint viewCount;
    uint meshCount;
    uint instanceCount;
} PushConstants;

void main() {

#ifdef OCCLUSION
    if (gl_GlobalInvocationID.x >= occlusionInstanceCount)
        return;
    uint idx = occlusionInstances[gl_GlobalInvocationID.x];
#else
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= PushConstants.instanceCount)
        return;
#endif

    uvec2 result = results[idx];
    uint viewMask = ((1u << PushConstants.viewCount) - 1u) << PushConstants.firstView;
    if ((result.x & viewMask) == 0u)
        return;

    Instance instance = instances[idx];
    CullMesh mesh = meshes[instance.meshIdx];
    uint lod = result.y >> RESULT_LOD_SHIFT;
    bool reversedWinding = (instance.flags & INSTANCE_REVERSED_WINDING) != 0u;

    for (uint i = PushConstants.firstView; i < PushConstants.firstView + PushConstants.viewCount; i++) {
        if ((result.x & (1u << i)) == 0u)
            continue;

        CullView view = views[i];
        bool impostor = (result.y & (1u << i)) != 0u;
        uint counterIdx = view.counterOffset + instance.meshIdx * MESH_COUNTER_COUNT + GetDrawCounter(impostor, lod, reversedWinding);
        uint position = atomicAdd(offsets[counterIdx], 1u);
        if (position < view.indexOffset + mesh.regionOffset + mesh.regionCapacity)
            indices[position] = idx;
    }

}
