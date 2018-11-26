layout (location = 0) out vec4 color;

#ifdef TEXTURE
in vec2 fTexCoord;
uniform sampler2D rectangleTexture;
#else
uniform vec4 rectangleColor;
#endif

void main() {
	
#ifdef TEXTURE
	color = texture(rectangleTexture, fTexCoord);
#else
	color = rectangleColor;
#endif

}