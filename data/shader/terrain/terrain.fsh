layout (location = 0) out vec3 diffuse;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec2 additional;

uniform mat4 vMatrix;

void main() {
	
	diffuse = vec3(0.01,1,0.01);
	normal = 0.5f * normalize(vec3(vMatrix * vec4(0.0f, 1.0f, 0.0f, 0.0f))) + 0.5f;
	additional = vec2(0.0f);
	
}