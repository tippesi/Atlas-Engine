#define SHADOW_FILTER_7x7
#define SHADOW_CASCADE_BLENDING

#include <../structures>
#include <../shadow.hsh>
#include <../fog.hsh>

#include <../common/convert.hsh>
#include <../common/material.hsh>
#include <../common/utility.hsh>
#include <../brdf/brdfEval.hsh>
#include <../common/octahedron.hsh>
#include <../common/utility.hsh>

layout(binding = 12) uniform sampler2DArray irradianceVolume;
layout(binding = 13) uniform sampler2DArray momentsVolume;

uniform vec3 volumeMin;
uniform vec3 volumeMax;
uniform ivec3 volumeProbeCount;

uniform int volumeIrradianceRes;
uniform int volumeMomentsRes;

vec3 GetIrradianceCoord(ivec3 probeIndex, vec3 dir) {

	vec2 totalResolution = vec2(volumeProbeCount.xz) * vec2(volumeIrradianceRes + 2);
	vec2 texelSize = 1.0 / totalResolution;

	vec2 irrRes = vec2(volumeIrradianceRes + 2);

	vec3 coord = vec3(irrRes * vec2(probeIndex.xz) + vec2(1.5), float(probeIndex.y));
	coord.xy *= texelSize;

	vec2 localCoord = UnitVectorToOctahedron(dir) * float(volumeIrradianceRes - 1) * texelSize;
	coord.xy += localCoord;

	return coord;

}

vec3 GetMomentsCoord(ivec3 probeIndex, vec3 dir) {

	vec2 totalResolution = vec2(volumeProbeCount.xz) * vec2(volumeMomentsRes + 2);
	vec2 texelSize = 1.0 / totalResolution;

	vec2 momRes = vec2(volumeMomentsRes + 2);

	vec3 coord = vec3(momRes * vec2(probeIndex.xz) + vec2(1.5), float(probeIndex.y));
	coord.xy *= texelSize;

	vec2 localCoord = UnitVectorToOctahedron(dir) * float(volumeMomentsRes - 1) * texelSize;
	coord.xy += localCoord;

	return coord;

}

vec4 GetLocalIrradiance(vec3 view, mat4 ivMatrix, vec3 N, out float blubWeight) {

	vec3 position = vec3(ivMatrix * vec4(view, 1.0));

	// Outside volume, only use sky probe
	// This is also a natural way to exist 
	// if there isn't any volume
	if (position.x <= volumeMin.x ||
		position.y <= volumeMin.y ||
		position.z <= volumeMin.z ||
		position.x >= volumeMax.x ||
		position.y >= volumeMax.y ||
		position.z >= volumeMax.z) {
		return vec4(0.0, 0.0, 0.0, 1.0);
	}

	vec3 volumeSize = volumeMax - volumeMin;
	vec3 cellSize = volumeSize / vec3(volumeProbeCount - ivec3(1));

	vec3 localPosition = position - volumeMin;
	ivec3 baseCell = ivec3(floor(localPosition / cellSize));

	float sumWeight = 0.0;
	vec4 sumIrradiance = vec4(0.0);

	float sumWeightNoCheb = 0.0;
	vec4 sumIrradianceNoCheb = vec4(0.0);

	vec3 alpha = localPosition / cellSize - vec3(baseCell);

	const float bias = 0.0;

	for (int i = 0; i < 8; i++) {
		ivec3 offset = ivec3(i, i >> 1, i >> 2) & ivec3(1);

		ivec3 gridCell = clamp(baseCell + offset, ivec3(0), volumeProbeCount - ivec3(1));

		vec3 probePos = vec3(gridCell) * cellSize + volumeMin;
		vec3 probeToPoint = position - probePos + N * bias;
		vec3 dir = normalize(-probeToPoint);
		float distToProbe = length(probeToPoint);

		vec3 trilinear = mix(vec3(1.0) - alpha, alpha, vec3(offset));

		// Creates pointlight like artefacts
		float weight = saturate(dot(dir, N));

		weight *= trilinear.x * trilinear.y * trilinear.z + 0.001;

		vec2 temp = texture(momentsVolume, GetMomentsCoord(gridCell, -dir)).rg;
		float mean = temp.x;
		float mean2 = temp.y;

		float chebWeight = 1.0;

		// Visibility (needs to be improved)
		if (distToProbe > mean) {
			float variance = abs(mean2 - sqr(mean));
			//weight *= variance / (variance + max(sqr(distToProbe - mean), 0.0));
		}

		float threshold = 0.1;
		if (weight < threshold) {
			//weight *= sqr(weight) / sqr(threshold);
		}

		vec4 irradiance = sqrt(texture(irradianceVolume, GetIrradianceCoord(gridCell, N)));

		sumWeight += weight;
		sumIrradiance += weight * irradiance;

	}

	return sqr(sumIrradiance / sumWeight);

}

in vec2 texCoordVS;
out vec4 colorFS;

layout(binding = 0) uniform sampler2D baseColorTexture;
layout(binding = 1) uniform sampler2D normalTexture;
layout(binding = 2) uniform sampler2D geometryNormalTexture;
layout(binding = 3) uniform sampler2D roughnessMetalnessAoTexture;
layout(binding = 4) uniform usampler2D materialIdxTexture;
layout(binding = 5) uniform sampler2D depthTexture;
layout(binding = 6) uniform sampler2D aoTexture;
layout(binding = 7) uniform sampler2D volumetricTexture;
layout(binding = 10) uniform samplerCube specularProbe;
layout(binding = 11) uniform samplerCube diffuseProbe;

uniform Light light;

uniform mat4 ivMatrix;
uniform vec3 cameraLocation;
uniform vec3 cameraDirection;

uniform float aoStrength;

void main() {
	
	float depth = texture(depthTexture, texCoordVS).r;
	
	if (depth == 1.0)
		discard;
	
	vec3 fragPos = ConvertDepthToViewSpace(depth, texCoordVS);	
	vec3 normal = normalize(2.0 * texture(normalTexture, texCoordVS).rgb - 1.0);

	uint materialIdx = texture(materialIdxTexture, texCoordVS).r;
	Material material = UnpackMaterial(materialIdx);

	vec3 geometryNormal = normalize(2.0 * texture(geometryNormalTexture, texCoordVS).rgb - 1.0);

	normal = material.normalMap ? normal : geometryNormal;

	if (material.baseColorMap) {
		material.baseColor *= texture(baseColorTexture, texCoordVS).rgb;
	}
	if (material.roughnessMap) {
		material.roughness *= texture(roughnessMetalnessAoTexture, texCoordVS).r;
	}
	if (material.metalnessMap) {
		material.metalness *= texture(roughnessMetalnessAoTexture, texCoordVS).g;
	}
	if (material.aoMap) {
		material.ao *= texture(roughnessMetalnessAoTexture, texCoordVS).b;
	}
	
	float shadowFactor = 1.0;
	vec3 volumetric = vec3(1.0);
	
	vec3 V = normalize(-fragPos);
	vec3 N = normal;
	vec3 L = -light.direction;

	if (material.transmissive) {
		N = dot(geometryNormal, V) < 0.0 ? -N : N;
	}

	Surface surface = CreateSurface(V, N, L, material);

	// Direct diffuse + specular BRDF
	vec3 directDiffuse = EvaluateDiffuseBRDF(surface);
	vec3 directSpecular = EvaluateSpecularBRDF(surface);

	vec3 direct = directDiffuse + directSpecular;

	// Indirect diffuse BRDF
	vec3 worldNormal = normalize(vec3(ivMatrix * vec4(geometryNormal, 0.0)));
	vec3 prefilteredDiffuse = texture(diffuseProbe, worldNormal).rgb;
	vec4 prefilteredDiffuseLocal = GetLocalIrradiance(fragPos, ivMatrix, worldNormal, sumWeight);
	prefilteredDiffuse = prefilteredDiffuseLocal.rgb + prefilteredDiffuse * prefilteredDiffuseLocal.a;
	vec3 indirectDiffuse = prefilteredDiffuse * EvaluateIndirectDiffuseBRDF(surface);

	// Indirect specular BRDF
	vec3 R = normalize(mat3(ivMatrix) * reflect(-surface.V, surface.N));
	float mipLevel = sqrt(material.roughness) * 9.0;
	vec3 prefilteredSpecular = textureLod(specularProbe, R, mipLevel).rgb;
	// We multiply by local sky visibility because the reflection probe only includes the sky
	vec3 indirectSpecular = prefilteredSpecular * EvaluateIndirectSpecularBRDF(surface) *
		prefilteredDiffuseLocal.a;

	vec3 indirect = (indirectDiffuse + indirectSpecular) * material.ao;
	
	if (material.transmissive) {
		Surface backSurface = CreateSurface(V, -N, L, material);

		// Direct diffuse BRDF backside
		direct += material.transmissiveColor * EvaluateDiffuseBRDF(backSurface);

		// Indirect diffuse BRDF backside
		prefilteredDiffuse = texture(diffuseProbe, -worldNormal).rgb;
		prefilteredDiffuseLocal = GetLocalIrradiance(fragPos, ivMatrix, -worldNormal, sumWeight);
		prefilteredDiffuse = prefilteredDiffuseLocal.rgb + prefilteredDiffuse * prefilteredDiffuseLocal.a;
		indirect += material.transmissiveColor * prefilteredDiffuse * 
			EvaluateIndirectDiffuseBRDF(backSurface) * material.ao;
	}

#ifdef SHADOWS	
	shadowFactor = CalculateCascadedShadow(light, fragPos,
		geometryNormal, 1.0 - max(surface.NdotL, 0.0)); 
	
	volumetric = vec3(texture(volumetricTexture, texCoordVS).r);
#endif
	
	float occlusionFactor = 1.0;
	
#ifdef SSAO
	occlusionFactor = pow(texture(aoTexture, texCoordVS).r, aoStrength);
	ambient *= occlusionFactor;
#endif
	
	vec3 radiance = light.color * light.intensity;
	colorFS = vec4(direct * radiance * surface.NdotL * shadowFactor + indirect, 1.0);

	if (length(material.emissiveColor) > 0.01) {	
		colorFS = vec4(material.emissiveColor, 1.0);
	}
	
	colorFS = vec4(applyFog(colorFS.rgb, length(fragPos), 
		cameraLocation, mat3(ivMatrix) * -surface.V, 
		mat3(ivMatrix) * -light.direction, light.color), 1.0);

}