#include <common.hsh>

layout (local_size_x = 64) in;

layout(std430, set = 3, binding = 0) writeonly buffer Instances {
    Instance instances[];
};

layout(std430, set = 3, binding = 1) readonly buffer Updates {
    Instance updates[];
};

layout(std430, set = 3, binding = 2) readonly buffer UpdateIndices {
    uint updateIndices[];
};

layout(push_constant) uniform constants {
    uint updateCount;
} PushConstants;

void main() {

    uint idx = gl_GlobalInvocationID.x;
    if (idx >= PushConstants.updateCount)
        return;

    instances[updateIndices[idx]] = updates[idx];

}
