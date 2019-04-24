#include "../common/material"

layout (location = 0) out vec3 diffuse;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec2 additional;

#ifndef GEOMETRY_SHADER
in vec2 teTexCoords;
in vec4 splat;
#endif

uniform mat4 vMatrix;
uniform sampler2D normalMap;

uniform Material materials[4];

void main() {
	
#ifndef GEOMETRY_SHADER	
	diffuse = texture(materials[0].diffuseMap, teTexCoords * 40.0f).rgb * splat.r + 
		texture(materials[1].diffuseMap, teTexCoords * 40.0f).rgb * splat.g + 
		texture(materials[2].diffuseMap, teTexCoords * 40.0f).rgb * splat.b + 
		texture(materials[3].diffuseMap, teTexCoords * 40.0f).rgb * splat.a;
	
	// We should move this to the tesselation evaluation shader
	// so we only have to calculate these normals once. After that 
	// we can pass a TBN matrix to this shader
	normal = 2.0f * texture(normalMap, teTexCoords).rgb - 1.0f;
	
	// vec3 tangent = vec3(1.0f, 0.0f, 0.0f);
	// tangent.y = -((normal.x*tangent.x) / normal.y) - ((normal.z*tangent.z) / normal.y);
	// tangent = normalize(tangent);	
	
	normal = 0.5f * normalize(vec3(vMatrix * vec4(normal, 0.0f))) + 0.5f;
	additional = vec2(0.0f);
#else
	normal = vec3(0.0f, 1.0f, 0.0f);
	diffuse = vec3(0.0f, 1.0f, 0.2f);
	additional = vec2(0.0f);
#endif
	
}