layout(location=0) in vec2 vPosition;

layout(location=0) out vec2 screenPositionVS;

#if defined(TEXTURE2D) || defined(TEXTURE2D_ARRAY) || defined(TEXTURE3D)
layout(location=1) out vec2 texCoordVS;
#endif

layout(push_constant) uniform constants {
	mat4 pMatrix;
	vec4 blendArea;
	vec4 clipArea;
	vec2 offset;
	vec2 scale;
	int invert;
	float depth;
} pushConstants;

void main() {
	
	vec2 position = (vPosition + 1.0) / 2.0;
	screenPositionVS = position * pushConstants.scale + pushConstants.offset;
    gl_Position = pushConstants.pMatrix * vec4(screenPositionVS, 0.0, 1.0);
	gl_Position.y *= -1.0f;
	
#if defined(TEXTURE2D) || defined(TEXTURE2D_ARRAY) || defined(TEXTURE3D)
	texCoordVS = position;
#endif
	
}