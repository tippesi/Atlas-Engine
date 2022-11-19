out vec4 color;

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
uniform bool invert = false;

void main() {

	if (fScreenPosition.x < rectangleClipArea.x ||
		fScreenPosition.y < rectangleClipArea.y ||
		fScreenPosition.x > rectangleClipArea.x + rectangleClipArea.z ||
		fScreenPosition.y > rectangleClipArea.y + rectangleClipArea.w)
		discard;

#if defined(TEXTURE2D) || defined(TEXTURE2D_ARRAY)
	vec2 texCoord = fTexCoord;

	if (invert) {
		texCoord.y = 1.0 - texCoord.y;
	}
#endif
	
#ifdef TEXTURE2D
	color = texture(rectangleTexture, vec2(texCoord));
#elif defined(TEXTURE2D_ARRAY)
	color = texture(rectangleTexture, vec3(texCoord, textureDepth));
#else
	color = rectangleColor;
#endif

}