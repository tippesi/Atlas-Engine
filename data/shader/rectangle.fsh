layout (location = 0) out vec4 color;

#ifdef TEXTURE2D
in vec2 fTexCoord;
layout(binding = 0) uniform sampler2D rectangleTexture;
#elif defined(TEXTURE2D_ARRAY)
in vec2 fTexCoord;
layout(binding = 0) uniform sampler2DArray rectangleTexture;
uniform float textureDepth;
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
	
#ifdef TEXTURE2D
	color = 1.0 - exp(-texture(rectangleTexture, fTexCoord));
#elif defined(TEXTURE2D_ARRAY)
	color = texture(rectangleTexture, vec3(fTexCoord, textureDepth));
#else
	color = rectangleColor;
#endif

}