layout (location = 0) out vec4 color;

#ifdef TEXTURE
in vec2 fTexCoord;
uniform sampler2D rectangleTexture;
#else
uniform vec4 rectangleColor;
#endif

in vec2 fScreenPosition;
uniform vec4 rectangleBlendArea;
uniform vec4 rectangleClipArea;

void main() {

	if (fScreenPosition.x < rectangleClipArea.x ||
		fScreenPosition.y < rectangleClipArea.y ||
		fScreenPosition.x > rectangleClipArea.x + rectangleClipArea.z ||
		fScreenPosition.y > rectangleClipArea.y + rectangleClipArea.w)
		discard;
	
#ifdef TEXTURE
	color = texture(rectangleTexture, fTexCoord);
#else
	color = rectangleColor;
#endif

}