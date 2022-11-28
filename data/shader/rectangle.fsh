out vec4 color;

#ifdef TEXTURE2D
in vec2 texCoordVS;
layout(binding = 0) uniform sampler2D rectangleTexture;
#elif defined(TEXTURE2D_ARRAY)
in vec2 texCoordVS;
layout(binding = 0) uniform sampler2DArray rectangleTexture;
#elif defined(TEXTURE3D)
in vec2 texCoordVS;
layout(binding = 0) uniform sampler3D rectangleTexture;
#else
uniform vec4 rectangleColor;
#endif

in vec2 screenPositionVS;
uniform vec4 blendArea;
uniform vec4 clipArea;
uniform bool invert = false;
uniform float depth;

void main() {

	if (screenPositionVS.x < clipArea.x ||
		screenPositionVS.y < clipArea.y ||
		screenPositionVS.x > clipArea.x + clipArea.z ||
		screenPositionVS.y > clipArea.y + clipArea.w)
		discard;

#if defined(TEXTURE2D) || defined(TEXTURE2D_ARRAY) || defined(TEXTURE3D)
	vec2 texCoord = texCoordVS;

	if (invert) {
		texCoord.y = 1.0 - texCoord.y;
	}
#endif
	
#ifdef TEXTURE2D
	color = texture(rectangleTexture, vec2(texCoord));
#elif defined(TEXTURE2D_ARRAY)
	color = texture(rectangleTexture, vec3(texCoord, depth));
#elif defined(TEXTURE3D)
	color = texture(rectangleTexture, vec3(texCoord, depth));
#else
	color = rectangleColor;
#endif

}