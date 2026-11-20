#include <common.hsh>

// Turns the counts of the draws of each mesh and view into offsets in the region of the mesh

layout (local_size_x = 64) in;

layout(std430, set = 3, binding = 0) readonly buffer Meshes {
    CullMesh meshes[];
};

layout(std430, set = 3, binding = 1) readonly buffer Views {
    CullView views[];
};

layout(std430, set = 3, binding = 2) readonly buffer Counters {
    uint counters[];
};

layout(std430, set = 3, binding = 3) writeonly buffer Offsets {
    uint offsets[];
};

layout(push_constant) uniform constants {
    uint firstView;
    uint viewCount;
    uint meshCount;
    uint instanceCount;
} PushConstants;

void main() {

    uint idx = gl_GlobalInvocationID.x;
    if (idx >= PushConstants.viewCount * PushConstants.meshCount)
        return;

    uint meshIdx = idx % PushConstants.meshCount;
    CullView view = views[PushConstants.firstView + idx / PushConstants.meshCount];

    uint counterIdx = view.counterOffset + meshIdx * MESH_COUNTER_COUNT;
    uint offset = view.indexOffset + meshes[meshIdx].regionOffset;
    for (uint i = 0u; i < MESH_COUNTER_COUNT; i++) {
        offsets[counterIdx + i] = offset;
        offset += counters[counterIdx + i];
    }

}
