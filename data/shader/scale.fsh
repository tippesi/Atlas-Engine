in vec2 fTexCoord;

uniform sampler2D diffuseMap;

out vec3 color;

void main() {
	
	color = texture(diffuseMap, fTexCoord).rgb;
   	
}