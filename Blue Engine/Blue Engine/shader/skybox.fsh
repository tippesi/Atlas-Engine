out vec3 fragColor;

in vec3 fTexCoord;

uniform samplerCube environmentCube;

void main() {
	
	fragColor = texture(environmentCube, fTexCoord).xyz;
	
}