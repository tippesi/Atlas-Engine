#include <terrainMaterial.hsh>
#include <../common/utility.hsh>

layout (location = 0) out vec3 baseColorFS;
layout (location = 1) out vec3 normalFS;
layout (location = 2) out vec3 geometryNormalFS;
layout (location = 3) out vec3 roughnessMetalnessAoFS;
layout (location = 4) out uint materialIdxFS;
layout (location = 5) out vec2 velocityFS;

layout (binding = 1) uniform sampler2D normalMap;
layout (binding = 2) uniform usampler2D splatMap;
layout (binding = 3) uniform sampler2DArray baseColorMaps;
layout (binding = 4) uniform sampler2DArray roughnessMaps;
layout (binding = 5) uniform sampler2DArray aoMaps;
layout (binding = 6) uniform sampler2DArray normalMaps;

layout (binding = 0, std140) uniform UBO {
    TerrainMaterial materials[128];
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
uniform float patchSize;
uniform float nodeSideLength;

uniform vec2 jitterLast;
uniform vec2 jitterCurrent;

vec3 SampleBaseColor(vec2 off, uvec4 indices, vec4 tiling) {
	
	vec3 q00 = texture(baseColorMaps, vec3(materialTexCoords / 4.0 * tiling.x, float(indices.x))).rgb;
	vec3 q10 = indices.y != indices.x ? texture(baseColorMaps, vec3(materialTexCoords / 4.0 * tiling.y, float(indices.y))).rgb : q00;
	vec3 q01 = indices.z != indices.x ? texture(baseColorMaps, vec3(materialTexCoords / 4.0 * tiling.z, float(indices.z))).rgb : q00;
	vec3 q11 = indices.w != indices.x ? texture(baseColorMaps, vec3(materialTexCoords / 4.0 * tiling.w, float(indices.w))).rgb : q00;
	
	// Interpolate samples horizontally
	vec3 h0 = mix(q00, q10, off.x);
	vec3 h1 = mix(q01, q11, off.x);
	
	// Interpolate samples vertically
	return mix(h0, h1, off.y);	
	
}

float SampleRoughness(vec2 off, uvec4 indices, vec4 tiling) {
	
	float q00 = texture(roughnessMaps, vec3(materialTexCoords / 4.0 * tiling.x, float(indices.x))).r;
	float q10 = texture(roughnessMaps, vec3(materialTexCoords / 4.0 * tiling.y, float(indices.y))).r;
	float q01 = texture(roughnessMaps, vec3(materialTexCoords / 4.0 * tiling.z, float(indices.z))).r;
	float q11 = texture(roughnessMaps, vec3(materialTexCoords / 4.0 * tiling.w, float(indices.w))).r;
	
	// Interpolate samples horizontally
	float h0 = mix(q00, q10, off.x);
	float h1 = mix(q01, q11, off.x);
	
	// Interpolate samples vertically
	return mix(h0, h1, off.y);	
	
}

float SampleAo(vec2 off, uvec4 indices, vec4 tiling) {
	
	float q00 = texture(aoMaps, vec3(materialTexCoords / 4.0 * tiling.x, float(indices.x))).r;
	float q10 = texture(aoMaps, vec3(materialTexCoords / 4.0 * tiling.y, float(indices.y))).r;
	float q01 = texture(aoMaps, vec3(materialTexCoords / 4.0 * tiling.z, float(indices.z))).r;
	float q11 = texture(aoMaps, vec3(materialTexCoords / 4.0 * tiling.w, float(indices.w))).r;
	
	// Interpolate samples horizontally
	float h0 = mix(q00, q10, off.x);
	float h1 = mix(q01, q11, off.x);
	
	// Interpolate samples vertically
	return mix(h0, h1, off.y);	
	
}

vec3 SampleNormal(vec2 off, uvec4 indices, vec4 tiling) {

	vec3 q00 = texture(normalMaps, vec3(materialTexCoords / 4.0 * tiling.x, float(indices.x))).rgb;
	vec3 q10 = texture(normalMaps, vec3(materialTexCoords / 4.0 * tiling.y, float(indices.y))).rgb;
	vec3 q01 = texture(normalMaps, vec3(materialTexCoords / 4.0 * tiling.z, float(indices.z))).rgb;
	vec3 q11 = texture(normalMaps, vec3(materialTexCoords / 4.0 * tiling.w, float(indices.w))).rgb;
	
	// Interpolate samples horizontally
	vec3 h0 = mix(q00, q10, off.x);
	vec3 h1 = mix(q01, q11, off.x);
	
	// Interpolate samples vertically
	return mix(h0, h1, off.y);	
	
}

float Interpolate(float q00, float q10, float q01, float q11, vec2 off) {

	// Interpolate samples horizontally
	float h0 = mix(q00, q10, off.x);
	float h1 = mix(q01, q11, off.x);
	
	// Interpolate samples vertically
	return mix(h0, h1, off.y);	

}

void main() {

	uvec4 indices;
	vec2 coords = materialTexCoords;

	vec2 tex = materialTexCoords / tileScale;	
	vec2 off = tex - floor(tex);

	off = vec2(off.x + (0.5 * sin(coords.y)
		+ 0.7 * cos(coords.y)) / tileScale,
		off.y + (0.4 * cos(coords.x * 2.0) + 0.6 * cos(coords.x)) / tileScale);
		
	vec2 splatOffset = floor(off);
	off = off - floor(off);

	float texel = 1.0 / (8.0 * patchSize);
	tex = (floor(coords / nodeSideLength / texel) + splatOffset) * texel;
	indices.x = textureLod(splatMap, tex, 0).r;
	indices.y = textureLod(splatMap, tex + vec2(texel, 0.0), 0).r;
	indices.z = textureLod(splatMap, tex + vec2(0.0, texel), 0).r;
	indices.w = textureLod(splatMap, tex + vec2(texel, texel), 0).r;
	
	vec4 tiling = vec4(
		materials[indices.x].tiling,
		materials[indices.y].tiling,
		materials[indices.z].tiling,
		materials[indices.w].tiling
	);

	baseColorFS = SampleBaseColor(off, indices, tiling);

	float roughness = Interpolate(
			materials[indices.x].roughness,
			materials[indices.y].roughness,
			materials[indices.z].roughness,
			materials[indices.w].roughness,
			off
		);
	float metalness = Interpolate(
			materials[indices.x].metalness,
			materials[indices.y].metalness,
			materials[indices.z].metalness,
			materials[indices.w].metalness,
			off
		);
	float ao = Interpolate(
			materials[indices.x].ao,
			materials[indices.y].ao,
			materials[indices.z].ao,
			materials[indices.w].ao,
			off
		);
	
	materialIdxFS = materials[indices.x].idx;

	// We should move this to the tesselation evaluation shader
	// so we only have to calculate these normals once. After that 
	// we can pass a TBN matrix to this shader
	tex = vec2(normalTexelSize) + texCoords * (1.0 - 3.0 * normalTexelSize)
		+ 0.5 * normalTexelSize;
	vec3 norm = 2.0 * texture(normalMap, tex).rgb - 1.0;

	geometryNormalFS = 0.5 * normalize(mat3(vMatrix) * norm) + 0.5;
	
#ifndef DISTANCE
	// Normal mapping only for near tiles
	float normalScale = Interpolate(
			materials[indices.x].normalScale,
			materials[indices.y].normalScale,
			materials[indices.z].normalScale,
			materials[indices.w].normalScale,
			off
		);
	normalFS = SampleNormal(off, indices, tiling);
	vec3 tang = vec3(1.0, 0.0, 0.0);
	tang.y = -((norm.x*tang.x) / norm.y) - ((norm.z*tang.z) / norm.y);
	tang = normalize(tang);
	vec3 bitang = normalize(cross(tang, norm));
	mat3 tbn = mat3(tang, bitang, norm);
	normalFS = normalize(tbn * (2.0 * normalFS - 1.0));
	normalFS = mix(norm, normalFS, normalScale);
	ao *= SampleAo(off, indices, tiling);
	roughness *= SampleRoughness(off, indices, tiling);
#else
	normalFS = norm;
#endif	
	
	normalFS = 0.5 * normalize(mat3(vMatrix) * normalFS) + 0.5;
	roughnessMetalnessAoFS = vec3(roughness, metalness, ao);
	
	// Calculate velocity
	vec2 ndcL = ndcLast.xy / ndcLast.z;
	vec2 ndcC = ndcCurrent.xy / ndcCurrent.z;

	ndcL -= jitterLast;
	ndcC -= jitterCurrent;

	velocityFS = (ndcL - ndcC) * 0.5;
	
}