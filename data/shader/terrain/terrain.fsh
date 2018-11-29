layout (location = 0) out vec3 diffuse;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec2 additional;

in vec2 teTexCoords;

uniform mat4 vMatrix;
uniform sampler2D normalMap;
uniform sampler2D diffuseMap;

void main() {
	
	diffuse = texture(diffuseMap, teTexCoords * 40.0f).rgb;
	
	// We should move this to the tesselation evaluation shader
	// so we only have to calculate these normals once. After that 
	// we can pass a TBN matrix to this shader
	normal = 2.0f * texture(normalMap, teTexCoords).rgb - 1.0f;
	
	// vec3 tangent = vec3(1.0f, 0.0f, 0.0f);
	// tangent.y = -((normal.x*tangent.x) / normal.y) - ((normal.z*tangent.z) / normal.y);
	// tangent = normalize(tangent);	
	
	normal = 0.5f * normalize(vec3(vMatrix * vec4(normal, 0.0f))) + 0.5f;
	additional = vec2(0.0f);
	
}