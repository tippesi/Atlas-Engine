#include <common.hsh>

// Writes the indirect draw commands of the culled instances, impostors are drawn without indices

layout (local_size_x = 64) in;

struct DrawRequest {
    uint meshIdx;
    uint view;
    uint type;
    uint indexCount;
    uint firstIndex;
    uint padding0;
    uint padding1;
    uint padding2;
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

layout(push_constant) uniform constants {
    uint requestCount;
} PushConstants;

void main() {

    uint idx = gl_GlobalInvocationID.x;
    if (idx >= PushConstants.requestCount)
        return;

    DrawRequest request = requests[idx];
    CullMesh mesh = meshes[request.meshIdx];
    CullView view = views[request.view];

    uint counterIdx = view.counterOffset + request.meshIdx * DRAW_TYPE_COUNT;
    uint reversedCount = min(counters[counterIdx + DRAW_REVERSED_WINDING], mesh.regionCapacity);
    uint defaultCount = min(counters[counterIdx + DRAW_DEFAULT_WINDING], mesh.regionCapacity - reversedCount);
    uint impostorCount = min(counters[counterIdx + DRAW_IMPOSTOR], mesh.regionCapacity);

    uint instanceCount = defaultCount;
    uint firstInstance = view.indexOffset + mesh.regionOffset;
    if (request.type == DRAW_REVERSED_WINDING) {
        instanceCount = reversedCount;
        firstInstance += mesh.regionCapacity - reversedCount;
    }
    else if (request.type == DRAW_IMPOSTOR) {
        instanceCount = impostorCount;
        firstInstance = view.impostorIndexOffset + mesh.impostorRegionOffset;
    }

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
        commands[offset + 3u] = 0u;
        commands[offset + 4u] = firstInstance;
    }

}
