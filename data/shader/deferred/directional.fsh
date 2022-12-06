#define SHADOW_FILTER_7x7
#define SHADOW_CASCADE_BLENDING

#include <deferred.hsh>

#include <../structures>
#include <../shadow.hsh>

#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../brdf/brdfEval.hsh>
#include <../common/octahedron.hsh>

layout (binding = 8) uniform sampler2DArrayShadow cascadeMaps;

in vec2 texCoordVS;
out vec4 colorFS;

uniform Light light;

uniform mat4 ivMatrix;
uniform vec3 cameraLocation;
uniform vec3 cameraDirection;
uniform bool sssEnabled = false;

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
	
#ifdef SHADOWS
	// Only need to test in the direction of the light and can the be used
	// for both the transmission and reflection. The inversion is only done
	// for transmissive materials
	vec3 shadowNormal = surface.material.transmissive ? dot(-light.direction, geometryNormal) < 0.0 ? 
		-geometryNormal : geometryNormal : geometryNormal;
	shadowFactor = CalculateCascadedShadow(light.shadow, cascadeMaps, surface.P,
		shadowNormal, saturate(dot(-light.direction, shadowNormal)));
	if (sssEnabled) {
		float sssFactor = textureLod(sssTexture, texCoordVS, 0).r;
		shadowFactor = min(sssFactor, shadowFactor);
	}
#endif
	
	vec3 radiance = light.color * light.intensity;
	colorFS = vec4(direct * radiance * surface.NdotL * shadowFactor, 1.0);

	if (surface.material.transmissive) {
		Surface backSurface = CreateSurface(surface.V, -surface.N, surface.L, surface.material);

		float viewDependency = saturate(dot(-surface.V, surface.L));
		viewDependency = sqr(viewDependency);

		// Direct diffuse BRDF backside
		direct = surface.material.transmissiveColor * EvaluateDiffuseBRDF(backSurface);
		colorFS += vec4(direct * radiance * backSurface.NdotL * shadowFactor, 1.0);
	}

	if (dot(surface.material.emissiveColor, vec3(1.0)) > 0.01) {	
		colorFS += vec4(surface.material.emissiveColor, 0.0);
	}

	//colorFS = vec4(surface.material.metalness);
}