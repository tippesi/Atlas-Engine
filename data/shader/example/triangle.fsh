layout (location = 0) out vec4 colorFS;

layout (location = 0) in vec2 texCoordVS;

layout(set = 0, binding = 0) uniform sampler2D colorTexture;

void main() {

    colorFS = vec4(texture(colorTexture, texCoordVS).rgb, 1.0f);

}