#define SHADOW_FILTER_7x7
#define SHADOW_CASCADE_BLENDING

#include <deferred.hsh>

#include <../structures>
#include <../shadow.hsh>
#include <../fog.hsh>

#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../brdf/brdfEval.hsh>
#include <../common/octahedron.hsh>

#include <../ddgi/ddgi.hsh>

vec4 GetLocalIrradiance(vec3 P, vec3 V, vec3 N, vec3 geometryNormal) {

	float minAxialDistance = min3(cellSize);
	vec3 biasedP = P + (0.3 * geometryNormal - 0.7 * V) * (0.75 * minAxialDistance) * volumeBias;

	vec2 irrRes = vec2(volumeIrradianceRes + 2);
	vec2 totalResolution = vec2(volumeProbeCount.xz) * irrRes;
	vec2 irrTexelSize = 1.0 / totalResolution;
	vec2 irrOctCoord = UnitVectorToOctahedron(N);

	vec2 momRes = vec2(volumeMomentsRes + 2);
	totalResolution = vec2(volumeProbeCount.xz) * momRes;
	vec2 momTexelSize = 1.0 / totalResolution;

	vec3 localPosition = biasedP - volumeMin;
	ivec3 baseCell = ivec3(localPosition / cellSize);

	float sumWeight = 0.0;
	vec3 sumIrradiance = vec3(0.0);

	float sumWeightCheb = 0.0;
	vec3 sumIrradianceCheb = vec3(0.0);

	vec3 alpha = localPosition / cellSize - vec3(baseCell);

	for (int i = 0; i < 8; i++) {
		ivec3 offset = ivec3(i, i >> 1, i >> 2) & ivec3(1);
		ivec3 gridCell = min(baseCell + offset, volumeProbeCount - ivec3(1));
		//if (offset != ivec3(1, 1, 0)) continue;
		uint probeState = probeStates[GetProbeIdx(gridCell)];
		if (probeState == PROBE_STATE_INACTIVE) continue;

		vec3 probePos = vec3(gridCell) * cellSize + volumeMin;
		vec3 pointToProbe = probePos - P;
		vec3 L = normalize(pointToProbe);

		vec3 biasedProbeToPosition = biasedP - probePos;
		float biasedDistToProbe = length(biasedProbeToPosition);
		biasedProbeToPosition /= biasedDistToProbe;

		vec3 trilinear = mix(vec3(1.0) - alpha, alpha, vec3(offset));

		// Should be used when the chebyshev test is weighted more
		//float weight = sqr((dot(L, geometryNormal) + 1.0) * 0.5) + 0.2;
		float weight = saturate(dot(L, geometryNormal));

		vec2 momOctCoord = UnitVectorToOctahedron(biasedProbeToPosition);
		vec3 momCoord = GetProbeCoord(gridCell, momOctCoord, momRes, momTexelSize, volumeMomentsRes);
		vec2 temp = textureLod(momentsVolume, momCoord, 0).rg;
		float mean = temp.x;
		float mean2 = temp.y;

		float weightCheb = weight;

		if (biasedDistToProbe > mean) {			
			float variance = abs(mean * mean - mean2);
			float visibility = variance / (variance + sqr(biasedDistToProbe - mean));
			weight *= max(0.05, visibility * visibility * visibility);
		}

		//weight = max(0.000001, weight);
		//weightCheb = max(0.000001, weightCheb);

		float trilinearWeight = trilinear.x * trilinear.y * trilinear.z;
		weight *= trilinearWeight;
		weightCheb *= trilinearWeight;

		vec3 irrCoord = GetProbeCoord(gridCell, irrOctCoord, irrRes, irrTexelSize, volumeIrradianceRes);
		vec3 irradiance = pow(textureLod(irradianceVolume, irrCoord, 0).rgb, vec3(0.5 * volumeGamma));

		sumIrradiance += weight * irradiance;
		sumIrradianceCheb += weightCheb * irradiance;
		sumWeight += weight;
		sumWeightCheb += weightCheb;
	}

	if (sumWeight == 0.0) return vec4(0.0, 0.0, 0.0, 0.0);	
	return vec4(mix(sqr(sumIrradianceCheb / sumWeightCheb),
		sqr(sumIrradiance / sumWeight), saturate(pow(sumWeight, 1.0 / 4.0))), 0.0);

}

in vec2 texCoordVS;
out vec4 colorFS;

uniform Light light;

uniform mat4 ivMatrix;
uniform vec3 cameraLocation;
uniform vec3 cameraDirection;

uniform float aoStrength = 4.0;

void main() {
	
	float depth = texture(depthTexture, texCoordVS).r;
	
	if (depth == 1.0)
		discard;

	vec3 geometryNormal;
	Surface surface = GetSurface(texCoordVS, depth, -light.direction, geometryNormal);
	
	float shadowFactor = 1.0;
	vec3 volumetric = vec3(1.0);

	// Direct diffuse + specular BRDF
	vec3 directDiffuse = EvaluateDiffuseBRDF(surface);
	vec3 directSpecular = EvaluateSpecularBRDF(surface);

	vec3 direct = directDiffuse + directSpecular;

	vec3 worldView = normalize(vec3(ivMatrix * vec4(surface.P, 0.0)));
	vec3 worldPosition = vec3(ivMatrix * vec4(surface.P, 1.0));
	vec3 worldNormal = normalize(vec3(ivMatrix * vec4(surface.N, 0.0)));
	vec3 geometryWorldNormal = normalize(vec3(ivMatrix * vec4(geometryNormal, 0.0)));

	// Indirect diffuse BRDF
	vec3 prefilteredDiffuse = texture(diffuseProbe, worldNormal).rgb;
	vec4 prefilteredDiffuseLocal = GetLocalIrradiance(worldPosition, worldView, worldNormal, geometryWorldNormal);
	prefilteredDiffuseLocal = IsInsideVolume(worldPosition) ? prefilteredDiffuseLocal : vec4(0.0, 0.0, 0.0, 1.0);
	prefilteredDiffuse = prefilteredDiffuseLocal.rgb + prefilteredDiffuse * prefilteredDiffuseLocal.a;
	vec3 indirectDiffuse = prefilteredDiffuse * EvaluateIndirectDiffuseBRDF(surface);

	// Indirect specular BRDF
	vec3 R = normalize(mat3(ivMatrix) * reflect(-surface.V, surface.N));
	float mipLevel = sqrt(surface.material.roughness) * 9.0;
	vec3 prefilteredSpecular = textureLod(specularProbe, R, mipLevel).rgb;
	// We multiply by local sky visibility because the reflection probe only includes the sky
	vec3 indirectSpecular = prefilteredSpecular * EvaluateIndirectSpecularBRDF(surface)
		* prefilteredDiffuseLocal.a;
	

	vec3 indirect = (indirectDiffuse + indirectSpecular) * surface.material.ao;
	/*
	if (surface.material.transmissive) {
		Surface backSurface = CreateSurface(surface.V, -surface.N, surface.L, surface.material);

		// Direct diffuse BRDF backside
		direct += surface.material.transmissiveColor * EvaluateDiffuseBRDF(backSurface);

		// Indirect diffuse BRDF backside
		prefilteredDiffuse = texture(diffuseProbe, -worldNormal).rgb;
		prefilteredDiffuseLocal = GetLocalIrradiance(surface.P, ivMatrix, -worldNormal);
		prefilteredDiffuse = prefilteredDiffuseLocal.rgb + prefilteredDiffuse * prefilteredDiffuseLocal.a;
		indirect += surfacce.material.transmissiveColor * prefilteredDiffuse * 
			EvaluateIndirectDiffuseBRDF(backSurface) * surface.material.ao;
	}
	*/
#ifdef SHADOWS
	shadowFactor = CalculateCascadedShadow(light, surface.P,
		geometryNormal, 1.0); 
	
	volumetric = vec3(texture(volumetricTexture, texCoordVS).r);
#endif
	
	float occlusionFactor = 1.0;
	
	occlusionFactor = pow(texture(aoTexture, texCoordVS).r, aoStrength);
	indirect *= occlusionFactor;
	
	vec3 radiance = light.color * light.intensity;
	colorFS = vec4(direct * radiance * surface.NdotL * shadowFactor + indirect + light.color * volumetric, 1.0);

	if (dot(surface.material.emissiveColor, vec3(1.0)) > 0.01) {	
		colorFS += vec4(surface.material.emissiveColor, 1.0);
	}
	
	colorFS = vec4(applyFog(colorFS.rgb, length(surface.P), 
		cameraLocation, mat3(ivMatrix) * -surface.V, 
		mat3(ivMatrix) * -light.direction, light.color), 1.0);

	/*
	ivec3 nearestProbe = GetNearestProbe(worldPosition);
	vec3 nearestProbePosition = GetProbePosition(nearestProbe);
	uint nearestProbeState = probeStates[GetProbeIdx(nearestProbe)];
	if (distance(nearestProbePosition, worldPosition) <= 0.1) {
		colorFS = nearestProbeState == PROBE_STATE_INACTIVE ? vec4(1.0, 0.0, 0.0, 1.0) : vec4(0.0, 1.0, 0.0, 1.0);
		colorFS = nearestProbeState == PROBE_STATE_NEW ? vec4(0.0, 0.0, 1.0, 1.0) : colorFS;
		//colorFS = vec4(vec3(float(nearestProbeState) / 100.0), 1.0);
	}
	*/
	
}