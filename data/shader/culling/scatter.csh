#include <common.hsh>

layout (local_size_x = 64) in;

layout(std430, set = 3, binding = 0) buffer Instances {
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

    uint instanceIdx = updateIndices[idx];
    Instance update = updates[idx];
    // Has to match GPUScene::keepVisibleLastFrame
    if (update.visibleLastFrame == 2u)
        update.visibleLastFrame = instances[instanceIdx].visibleLastFrame;
    instances[instanceIdx] = update;

}
