#include "terrainMaterial"

layout (location = 0) out vec3 diffuse;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec2 additional;
layout (location = 3) out vec2 velocity;

layout (binding = 1) uniform sampler2D normalMap;
layout (binding = 2)  uniform usampler2D splatMap;
layout (binding = 3) uniform sampler2DArray diffuseMaps;
layout (binding = 4) uniform sampler2DArray normalMaps;

layout (binding = 0, std140) uniform UBO {
    TerrainMaterial materials[256];
};

in vec2 texCoords;
in vec2 bilinearPosition;
in vec2 materialTexCoords;
in vec3 ndcCurrent;
in vec3 ndcLast;

uniform mat4 vMatrix;
uniform float normalTexelSize;
uniform float normalResFactor;
uniform float patchSize;
uniform float nodeSideLength;

uniform vec2 jitterLast;
uniform vec2 jitterCurrent;

vec3 SampleDiffuse(vec2 off, uvec4 indices) {
	
	if (indices.x == indices.y && 
		indices.x == indices.z &&
		indices.x == indices.w)
		return texture(diffuseMaps, vec3(materialTexCoords / 4.0, float(indices.x))).rgb;

	vec3 q00 = texture(diffuseMaps, vec3(materialTexCoords / 4.0, float(indices.x))).rgb;
	vec3 q10 = texture(diffuseMaps, vec3(materialTexCoords / 4.0, float(indices.y))).rgb;
	vec3 q01 = texture(diffuseMaps, vec3(materialTexCoords / 4.0, float(indices.z))).rgb;
	vec3 q11 = texture(diffuseMaps, vec3(materialTexCoords / 4.0, float(indices.w))).rgb;
	
	// Interpolate samples horizontally
	vec3 h0 = mix(q00, q10, off.x);
	vec3 h1 = mix(q01, q11, off.x);
	
	// Interpolate samples vertically
	return mix(h0, h1, off.y);	
	
}

void main() {
	
	vec2 off = bilinearPosition - floor(bilinearPosition);

	float texel = 1.0 / (8.0 * patchSize);
	vec2 tex = floor(texCoords / texel) * texel;
	
	uvec4 indices;
	indices.x = texture(splatMap, tex).r;
	indices.y = texture(splatMap, tex + vec2(texel, 0.0)).r;
	indices.z = texture(splatMap, tex + vec2(0.0, texel)).r;
	indices.w = texture(splatMap, tex + vec2(texel, texel)).r;
	
	diffuse = SampleDiffuse(off, indices);
	
	float specularIntensity = materials[indices.x].specularIntensity;		
	float specularHardness = materials[indices.x].specularHardness;

	tex = vec2(normalTexelSize) + texCoords * (1.0 - 3.0 * normalTexelSize)
		+ 0.5 * normalTexelSize;
	vec3 norm = 2.0 * texture(normalMap, tex).rgb - 1.0;
	
	normal = 0.5 * normalize(vec3(vMatrix * vec4(norm, 0.0))) + 0.5;
	additional = vec2(specularIntensity, specularHardness);
	
	// Calculate velocity
	vec2 ndcL = ndcLast.xy / ndcLast.z;
	vec2 ndcC = ndcCurrent.xy / ndcCurrent.z;

	ndcL -= jitterLast;
	ndcC -= jitterCurrent;

	velocity = (ndcL - ndcC) * vec2(0.5, 0.5);
	
}