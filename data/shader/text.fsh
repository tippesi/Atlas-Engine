layout (location = 0) out vec4 color;

in vec3 fTexCoord;

uniform sampler2DArray glyphsTexture;
uniform vec4 textColor;
uniform bool outline;
uniform vec4 outlineColor;
uniform float outlineScale;
uniform float pixelDistanceScale;
uniform int edgeValue;

void main() {
	
	float intensity = texture(glyphsTexture, fTexCoord).r;
	
	if (intensity < (edgeValue-pixelDistanceScale*outlineScale)/255.0f && outline)
		discard;
	else if (intensity < edgeValue/255.0f && !outline)
		discard;
	
	if (intensity >= (edgeValue-pixelDistanceScale*outlineScale)/255.0f && outline)
		color = outlineColor;
	
	if (intensity >= edgeValue/255.0f)
		color = textColor;
	
}