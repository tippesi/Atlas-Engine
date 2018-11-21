layout (location = 0) out vec3 diffuse;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec2 additional;

in vec2 teTexCoords;

uniform mat4 vMatrix;
uniform sampler2D normalMap;

void main() {
	
	diffuse = vec3(0.01,1,0.01);
	normal = 2.0f * texture(normalMap, teTexCoords).rgb - 1.0f;
	normal = 0.5f * normalize(vec3(vMatrix * vec4(normal, 0.0f))) + 0.5f;
	additional = vec2(0.0f);
	
}