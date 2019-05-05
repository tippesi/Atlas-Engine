out vec3 fragColor;

in vec3 fTexCoord;

layout(binding = 0) uniform samplerCube skyCubemap;

void main() {
	
	fragColor = texture(skyCubemap, fTexCoord).xyz;
	
}