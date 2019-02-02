layout(location=0)in vec2 vPosition;

#if defined(TEXTURE2D) || defined(TEXTURE2D_ARRAY)
out vec2 fTexCoord;
#endif

out vec2 fScreenPosition;

uniform mat4 pMatrix;

uniform vec2 rectangleOffset;
uniform vec2 rectangleScale;

void main() {
	
	vec2 position = (vPosition + 1.0) / 2.0;
	fScreenPosition = position * rectangleScale + rectangleOffset;
    gl_Position = pMatrix * vec4(fScreenPosition, 0.0, 1.0);
	gl_Position.y *= -1.0f;
	
#if defined(TEXTURE2D) || defined(TEXTURE2D_ARRAY)
	fTexCoord = position;
#endif
	
}