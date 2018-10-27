in vec2 fTexCoord;

uniform sampler2D diffuseMap;
uniform vec3 bloomThreshold;
uniform float bloomPower;

out vec3 color;

void main() {
	
	vec3 fragColor = texture(diffuseMap, fTexCoord).rgb;
	
	float bloomBrightness = pow(dot(fragColor.xyz, bloomThreshold.xyz), bloomPower);
	color = fragColor.xyz * bloomBrightness;
   	
}