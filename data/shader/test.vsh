layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in uint inFlags;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out flat uint fragFlags;

layout (std140, binding=0) uniform Matrices
{
    mat4 projection;
    mat4 view;
    uint uniformFlags;
};

void main() {
    gl_Position = projection * view * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragNormal = inNormal;
    fragFlags = inFlags;
}
