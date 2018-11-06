out vec3 fragColor;

in vec3 fTexCoord;

uniform samplerCube skyCubemap;

void main() {
	
	fragColor = texture(skyCubemap, fTexCoord).xyz;
	
}