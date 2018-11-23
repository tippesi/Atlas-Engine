layout (location = 0) out vec4 color;

in vec3 fTexCoord;

uniform sampler2DArray glyphsTexture;
uniform vec4 textColor;
uniform float pixelDistanceScale;
uniform int edgeValue;

void main() {
	float intensity = texture(glyphsTexture, fTexCoord).r;
	/*
	if (intensity >= (edgeValue-pixelDistanceScale*1.0f)/255.0f)
		color = vec4(1.0f);
	*/
	if (intensity >= edgeValue/255.0f) 
		color = textColor;
    
}