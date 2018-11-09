in vec2 fTexCoord;

uniform sampler2D diffuseMap;
uniform vec2 blurScale;

uniform float offset[80];
uniform float weight[80];

uniform int kernelSize;

out vec3 color;

void main() {
	
	color = texture(diffuseMap, fTexCoord).rgb * weight[0];
	
	for(int i = 1; i < kernelSize; i++)		
		color += texture(diffuseMap, fTexCoord + (vec2(offset[i]) * blurScale.xy)).rgb * weight[i];
	
	for (int i = 1; i < kernelSize; i++)
		color += texture(diffuseMap, fTexCoord - (vec2(offset[i]) * blurScale.xy)).rgb * weight[i];
   	
}