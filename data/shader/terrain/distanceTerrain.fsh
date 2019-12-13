#include "../common/material"

layout (location = 0) out vec3 diffuse;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec2 additional;
layout (location = 3) out vec2 velocity;

in vec2 texCoords;
in vec2 materialTexCoords;
in vec4 splat;
in vec3 ndcCurrent;
in vec3 ndcLast;

uniform mat4 vMatrix;
uniform sampler2D normalMap;
uniform float normalTexelSize;
uniform float normalResFactor;

uniform Material materials[4];

uniform vec2 jitterLast;
uniform vec2 jitterCurrent;

void main() {
	
	diffuse = texture(materials[0].diffuseMap, materialTexCoords / 4.0).rgb * splat.r + 
		texture(materials[1].diffuseMap, materialTexCoords / 4.0).rgb * splat.g + 
		texture(materials[2].diffuseMap, materialTexCoords / 4.0).rgb * splat.b + 
		texture(materials[3].diffuseMap, materialTexCoords / 4.0).rgb * splat.a;

	vec2 tex = vec2(normalTexelSize) + texCoords * (1.0 - 3.0 * normalTexelSize)
		+ 0.5 * normalTexelSize;
	vec3 norm = 2.0 * texture(normalMap, tex).rgb - 1.0;
	
	normal = 0.5 * normalize(vec3(vMatrix * vec4(norm, 0.0))) + 0.5;
	additional = vec2(0.0);
	
	// Calculate velocity
	vec2 ndcL = ndcLast.xy / ndcLast.z;
	vec2 ndcC = ndcCurrent.xy / ndcCurrent.z;

	ndcL -= jitterLast;
	ndcC -= jitterCurrent;

	velocity = (ndcL - ndcC) * vec2(0.5, 0.5);
	
}