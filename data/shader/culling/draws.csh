#include <common.hsh>

// Writes the indirect draw commands of the culled instances, impostors are drawn without indices

layout (local_size_x = 64) in;

struct DrawRequest {
    uint meshIdx;
    uint view;
    uint type;
    uint indexCount;
    uint firstIndex;
    uint lod;
    int vertexOffset;
    uint padding0;
};

layout(std430, set = 3, binding = 0) readonly buffer Meshes {
    CullMesh meshes[];
};

layout(std430, set = 3, binding = 1) readonly buffer Views {
    CullView views[];
};

layout(std430, set = 3, binding = 2) readonly buffer Counters {
    uint counters[];
};

layout(std430, set = 3, binding = 3) readonly buffer Requests {
    DrawRequest requests[];
};

layout(std430, set = 3, binding = 4) writeonly buffer Commands {
    uint commands[];
};

// The end of the indices of each draw
layout(std430, set = 3, binding = 5) readonly buffer Offsets {
    uint offsets[];
};

layout(push_constant) uniform constants {
    uint firstRequest;
    uint requestCount;
} PushConstants;

void main() {

    if (gl_GlobalInvocationID.x >= PushConstants.requestCount)
        return;

    uint idx = PushConstants.firstRequest + gl_GlobalInvocationID.x;
    DrawRequest request = requests[idx];
    CullMesh mesh = meshes[request.meshIdx];
    CullView view = views[request.view];

    uint counterIdx = view.counterOffset + request.meshIdx * MESH_COUNTER_COUNT +
        GetDrawCounter(request.type == DRAW_IMPOSTOR, request.lod, request.type == DRAW_REVERSED_WINDING);
    uint instanceCount = counters[counterIdx];
    uint firstInstance = offsets[counterIdx] - instanceCount;

    // Indices beyond the region weren't written
    uint regionEnd = view.indexOffset + mesh.regionOffset + mesh.regionCapacity;
    instanceCount = min(instanceCount, regionEnd - min(firstInstance, regionEnd));

    uint offset = idx * 5u;
    if (request.type == DRAW_IMPOSTOR) {
        // A quad per impostor
        commands[offset + 0u] = 4u;
        commands[offset + 1u] = instanceCount;
        commands[offset + 2u] = 0u;
        commands[offset + 3u] = firstInstance;
        commands[offset + 4u] = 0u;
    }
    else {
        commands[offset + 0u] = request.indexCount;
        commands[offset + 1u] = instanceCount;
        commands[offset + 2u] = request.firstIndex;
        commands[offset + 3u] = uint(request.vertexOffset);
        commands[offset + 4u] = firstInstance;
    }

}
