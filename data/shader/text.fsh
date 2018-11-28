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

void main() {

    if (fScreenPosition.x < clipArea.x ||
		fScreenPosition.y < clipArea.y ||
        fScreenPosition.x > clipArea.x + clipArea.z ||
        fScreenPosition.y > clipArea.y + clipArea.w)
            discard;
	
	float intensity = texture(glyphsTexture, fTexCoord).r;
	
	if (intensity < (edgeValue - pixelDistanceScale * outlineScale) / 255.0f && outline)
		discard;
	else if (intensity < edgeValue/255.0f && !outline)
		discard;
	
	if (intensity >= (edgeValue - pixelDistanceScale * outlineScale) / 255.0f && outline)
		color = outlineColor;
	
	if (intensity >= edgeValue / 255.0f)
		color = textColor;
	
}