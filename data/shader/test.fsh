//output write
layout (location = 0) out vec4 outFragColor;

layout (location = 0) in vec2 texCoordVS;

layout(set = 0, binding = 0) uniform sampler2D hdrTexture;

#define AUTO
#ifdef AUTO
layout(set = 1, binding = 0) uniform sampler2D otherTexture;
#endif

void main() {

	outFragColor = vec4(texture(hdrTexture, texCoordVS).rgb, 1.0f);

}