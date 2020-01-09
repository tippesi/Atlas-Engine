#include "terrainMaterial"

layout (location = 0) out vec3 diffuse;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec2 additional;
layout (location = 3) out vec2 velocity;

layout (binding = 1) uniform sampler2D normalMap;

layout (binding = 3) uniform sampler2DArray diffuseMaps;
layout (binding = 4) uniform sampler2DArray normalMaps;

layout (binding = 0, std140) uniform UBO {
    TerrainMaterial materials[256];
};

in vec2 materialTexCoords;
in vec2 texCoords;
in vec3 ndcCurrent;
in vec3 ndcLast;
flat in uvec4 materialIndicesTE;

uniform mat4 vMatrix;
uniform float normalTexelSize;
uniform float normalResFactor;
uniform float tileScale;

uniform vec2 jitterLast;
uniform vec2 jitterCurrent;

vec3 SampleDiffuse(vec2 off) {
	
	if (materialIndicesTE.x == materialIndicesTE.y && 
		materialIndicesTE.x == materialIndicesTE.z &&
		materialIndicesTE.x == materialIndicesTE.w)
		return texture(diffuseMaps, vec3(materialTexCoords / 4.0, float(materialIndicesTE.x))).rgb;

	vec3 q00 = texture(diffuseMaps, vec3(materialTexCoords / 4.0, float(materialIndicesTE.x))).rgb;
	vec3 q10 = texture(diffuseMaps, vec3(materialTexCoords / 4.0, float(materialIndicesTE.y))).rgb;
	vec3 q01 = texture(diffuseMaps, vec3(materialTexCoords / 4.0, float(materialIndicesTE.z))).rgb;
	vec3 q11 = texture(diffuseMaps, vec3(materialTexCoords / 4.0, float(materialIndicesTE.w))).rgb;
	
	// Interpolate samples horizontally
	vec3 h0 = mix(q00, q10, off.x);
	vec3 h1 = mix(q01, q11, off.x);
	
	// Interpolate samples vertically
	return mix(h0, h1, off.y);	
	
}

vec3 SampleNormal(vec2 off) {

	if (materialIndicesTE.x == materialIndicesTE.y && 
		materialIndicesTE.x == materialIndicesTE.z &&
		materialIndicesTE.x == materialIndicesTE.w)
		return texture(normalMaps, vec3(materialTexCoords / 4.0, float(materialIndicesTE.x))).rgb;
	
	vec3 q00 = texture(normalMaps, vec3(materialTexCoords / 4.0, float(materialIndicesTE.x))).rgb;
	vec3 q10 = texture(normalMaps, vec3(materialTexCoords / 4.0, float(materialIndicesTE.y))).rgb;
	vec3 q01 = texture(normalMaps, vec3(materialTexCoords / 4.0, float(materialIndicesTE.z))).rgb;
	vec3 q11 = texture(normalMaps, vec3(materialTexCoords / 4.0, float(materialIndicesTE.w))).rgb;
	
	// Interpolate samples horizontally
	vec3 h0 = mix(q00, q10, off.x);
	vec3 h1 = mix(q01, q11, off.x);
	
	// Interpolate samples vertically
	return mix(h0, h1, off.y);	
	
}

void main() {

	uint materialIndex = materialIndicesTE.x;
	
	vec2 tex = materialTexCoords / tileScale;	
	vec2 off = (tex) - floor(tex);
	
	diffuse = SampleDiffuse(off);

	float specularIntensity = materials[materialIndex].specularIntensity;		
	float specularHardness = materials[materialIndex].specularHardness;
	
	// We should move this to the tesselation evaluation shader
	// so we only have to calculate these normals once. After that 
	// we can pass a TBN matrix to this shader
	tex = vec2(normalTexelSize) + texCoords * (1.0 - 3.0 * normalTexelSize)
		+ 0.5 * normalTexelSize;
	vec3 norm = 2.0 * texture(normalMap, tex).rgb - 1.0;
	
#ifndef DISTANCE
	// Normal mapping only for near tiles
	float normalScale = materials[materialIndex].normalScale;
	normal = SampleNormal(off);
	vec3 tang = vec3(1.0, 0.0, 0.0);
	tang.y = -((norm.x*tang.x) / norm.y) - ((norm.z*tang.z) / norm.y);
	tang = normalize(tang);
	vec3 bitang = normalize(cross(tang, norm));
	mat3 tbn = mat3(tang, bitang, norm);
	normal = normalize(tbn * (2.0 * normal - 1.0));
	normal = mix(norm, normal, normalScale);
#else
	normal = norm;
#endif
	
	normal = 0.5 * normalize(mat3(vMatrix) * normal) + 0.5;
	additional = vec2(specularIntensity, specularHardness);
	
	// Calculate velocity
	vec2 ndcL = ndcLast.xy / ndcLast.z;
	vec2 ndcC = ndcCurrent.xy / ndcCurrent.z;

	ndcL -= jitterLast;
	ndcC -= jitterCurrent;

	velocity = (ndcL - ndcC) * 0.5;
	
}