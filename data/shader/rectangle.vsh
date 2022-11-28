layout(location=0) in vec2 vPosition;

#if defined(TEXTURE2D) || defined(TEXTURE2D_ARRAY) || defined(TEXTURE3D)
out vec2 texCoordVS;
#endif

out vec2 screenPositionVS;

uniform mat4 pMatrix;

uniform vec2 offset;
uniform vec2 scale;

void main() {
	
	vec2 position = (vPosition + 1.0) / 2.0;
	screenPositionVS = position * scale + offset;
    gl_Position = pMatrix * vec4(screenPositionVS, 0.0, 1.0);
	gl_Position.y *= -1.0f;
	
#if defined(TEXTURE2D) || defined(TEXTURE2D_ARRAY)
	texCoordVS = position;
#endif
	
}