layout (location = 0) out vec4 color;

in vec3 fTexCoord;
in vec2 fScreenPosition;

uniform sampler2DArray glyphsTexture;
uniform vec4 textColor;
uniform bool outline;
uniform vec4 outlineColor;
uniform float outlineScale;
uniform float pixelDistanceScale;
uniform int edgeValue;

uniform vec4 clipArea;
uniform vec4 blendArea;

uniform float textScale;

void main() {

	color = vec4(0.0f);
    float factor = 5.0f * textScale;

    if (fScreenPosition.x < clipArea.x ||
		fScreenPosition.y < clipArea.y ||
        fScreenPosition.x > clipArea.x + clipArea.z ||
        fScreenPosition.y > clipArea.y + clipArea.w)
            discard;
	
	float intensity = texture(glyphsTexture, fTexCoord).r;

	float outlineDistance = (float(edgeValue) - pixelDistanceScale * outlineScale) / 255.0f - intensity;
	float inlineDistance = float(edgeValue) / 255.0f - intensity;

	float outlineMultiplier = clamp(0.5f - factor * outlineDistance, 0.0f, 1.0f);
    float inlineMultiplier = clamp(0.5f - factor * inlineDistance, 0.0f, 1.0f);

	if (outlineMultiplier == 0.0f && outline)
		discard;
	else if (inlineMultiplier == 0.0f && !outline)
		discard;
	
	if (outlineMultiplier >= 0.0f && outline)
		color = vec4(outlineColor.rgb, outlineColor.a * outlineMultiplier);
	
	color = mix(color, vec4(textColor.rgb, textColor.a * inlineMultiplier), inlineMultiplier);

}