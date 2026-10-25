#include <skinning.hsh>

layout (local_size_x = 64) in;

layout(std430, set = 3, binding = 2) writeonly buffer Positions {
    float positions[];
};

layout(push_constant) uniform constants {
    uint elementCount;
    uint jointOffset;
} PushConstants;

void main() {

    uint idx = gl_GlobalInvocationID.x;
    if (idx >= PushConstants.elementCount)
        return;

    mat3x4 skinMatrix = GetSkinMatrix(idx, PushConstants.jointOffset);

    vec3 position = vec4(skinVertices[idx].position.xyz, 1.0) * skinMatrix;

    positions[idx * 3 + 0] = position.x;
    positions[idx * 3 + 1] = position.y;
    positions[idx * 3 + 2] = position.z;

}
