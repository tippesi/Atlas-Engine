#define SHADOW_FILTER_7x7
#define SHADOW_CASCADE_BLENDING

#include <deferred.hsh>

#include <../structures>
#include <../shadow.hsh>

#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../brdf/brdfEval.hsh>
#include <../common/octahedron.hsh>

in vec2 texCoordVS;
out vec4 colorFS;

uniform Light light;

uniform mat4 ivMatrix;
uniform vec3 cameraLocation;
uniform vec3 cameraDirection;

void main() {
	
	float depth = textureLod(depthTexture, texCoordVS, 0).r;
	
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
		geometryNormal, saturate(dot(-light.direction, geometryNormal))); 
#endif
	
	vec3 radiance = light.color * light.intensity;
	colorFS = vec4(direct * radiance * surface.NdotL * shadowFactor, 1.0);

	if (dot(surface.material.emissiveColor, vec3(1.0)) > 0.01) {	
		colorFS += vec4(surface.material.emissiveColor, 0.0);
	}

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