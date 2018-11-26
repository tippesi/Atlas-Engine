layout(location=0)in vec2 vPosition;

#ifdef TEXTURE
out vec2 fTexCoord;
#endif

uniform mat4 pMatrix;

uniform vec2 rectangleOffset;
uniform vec2 rectangleScale;

void main() {
	
	vec2 position = (vPosition + 1.0) / 2.0;
    gl_Position = pMatrix * vec4(position * rectangleScale + rectangleOffset, 0.0, 1.0);
	gl_Position.y *= -1.0f;
	
#ifdef TEXTURE
	fTexCoord = position;
#endif
	
}