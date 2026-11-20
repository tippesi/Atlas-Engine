layout (local_size_x = 64) in;

#include <common.hsh>
#include <sharedUniforms.hsh>

layout(std430, set = 3, binding = 14) readonly buffer Positions {
    vec4 positions[];
};

layout(std430, set = 3, binding = 18) writeonly buffer Results {
    vec4 results[];
};

layout (set = 3, binding = 17, std140) uniform QueryUniformBuffer {
    int count;
} QueryUniforms;

vec3 GetDisplacedPosition(vec3 position) {

    float perlinScale, shoreScale;
    vec3 normalShoreWave;
    // The physics shouldn't depend on the camera, so the waves don't fade out with the distance to it
    return position + GetOceanDisplacement(position, 0.0, perlinScale, shoreScale, normalShoreWave);

}

void main() {

    int idx = int(gl_GlobalInvocationID.x);
    if (idx >= QueryUniforms.count)
        return;

    vec2 position = positions[idx].xz;

    // The waves move the surface sideways as well, search the undisplaced position that ends up here
    vec3 originalPosition = vec3(position.x, Uniforms.translation.y, position.y);
    for (int i = 0; i < 4; i++) {
        vec3 displacedPosition = GetDisplacedPosition(originalPosition);
        originalPosition.xz += position - displacedPosition.xz;
    }

    vec3 displacedPosition = GetDisplacedPosition(originalPosition);
    vec3 tangent = GetDisplacedPosition(originalPosition + vec3(0.1, 0.0, 0.0)) - displacedPosition;
    vec3 bitangent = GetDisplacedPosition(originalPosition + vec3(0.0, 0.0, 0.1)) - displacedPosition;

    results[idx] = vec4(normalize(cross(bitangent, tangent)), displacedPosition.y);

}
