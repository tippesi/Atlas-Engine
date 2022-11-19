#define SHADOW_FILTER_1x1

#include <../raytracer/lights.hsh>
#include <../raytracer/tracing.hsh>
#include <../raytracer/direct.hsh>

#include <../common/random.hsh>
#include <../common/utility.hsh>
#include <../common/flatten.hsh>
#include <../common/convert.hsh>
#include <../common/PI.hsh>

#include <../brdf/brdfEval.hsh>
#include <../brdf/importanceSample.hsh>
#include <../brdf/surface.hsh>

#include <../ddgi/ddgi.hsh>
#include <../shadow.hsh>

layout (local_size_x = 8, local_size_y = 4) in;

layout (binding = 4, rgba16f) writeonly uniform image2D rtrImage;

layout(binding = 16) uniform sampler2D normalTexture;
layout(binding = 17) uniform sampler2D depthTexture;
layout(binding = 18) uniform sampler2D roughnessMetallicAoTexture;
layout(binding = 19) uniform isampler2D offsetTexture;
layout(binding = 20) uniform usampler2D materialIdxTexture;
layout(binding = 21) uniform sampler2D randomTexture;
layout(binding = 26) uniform sampler2DArrayShadow cascadeMaps;

const ivec2 offsets[4] = ivec2[4](
    ivec2(0, 0),
    ivec2(1, 0),
    ivec2(0, 1),
    ivec2(1, 1)
);

uniform int sampleCount;
uniform float radianceLimit;

uniform mat4 pMatrix;
uniform mat4 ivMatrix;

uniform ivec2 resolution;
uniform uint frameSeed;

uniform vec2 jitter;
uniform float bias;

#ifdef USE_SHADOW_MAP
uniform Shadow shadow;
#endif

vec3 EvaluateHit(inout Ray ray);
vec3 EvaluateDirectLight(inout Surface surface);
bool CheckVisibility(Surface surface, float lightDistance);

void main() {

	if (int(gl_GlobalInvocationID.x) < resolution.x &&
		int(gl_GlobalInvocationID.y) < resolution.y) {

		ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
		
		vec2 texCoord = (vec2(pixel) + vec2(0.5)) / vec2(resolution);

        int offsetIdx = texelFetch(offsetTexture, pixel, 0).r;
        ivec2 offset = offsets[offsetIdx];

        float depth = texelFetch(depthTexture, pixel, 0).r;

        vec2 recontructTexCoord = (2.0 * vec2(pixel) + offset + vec2(0.5)) / (2.0 * vec2(resolution));
        vec3 viewPos = ConvertDepthToViewSpace(depth, recontructTexCoord);
	    vec3 worldPos = vec3(ivMatrix * vec4(viewPos, 1.0));
        vec3 viewVec = vec3(ivMatrix * vec4(viewPos, 0.0));
        vec3 worldNorm = normalize(vec3(ivMatrix * vec4(2.0 * textureLod(normalTexture, texCoord, 0).rgb - 1.0, 0.0)));
		
		ivec2 noiseOffset = Unflatten2D(int(frameSeed), ivec2(16)) * ivec2(8);
        vec2 blueNoiseVec = texelFetch(randomTexture, (pixel + noiseOffset) % ivec2(128), 0).xy * 256.0;
		blueNoiseVec = clamp(blueNoiseVec, 0.0, 255.0);
		blueNoiseVec = (blueNoiseVec + 0.5) / 256.0;

        uint materialIdx = texelFetch(materialIdxTexture, pixel, 0).r;
	    Material material = UnpackMaterial(materialIdx);

        float roughness = texelFetch(roughnessMetallicAoTexture, pixel, 0).r;
        material.roughness *= material.roughnessMap ? roughness : 1.0;

        vec3 reflection = vec3(0.0);

        if (material.roughness < 1.0) {

            for (uint i = 0; i < sampleCount; i++) {
                Ray ray;

				float alpha = sqr(material.roughness);
				float biasedAlpha = (1.0 - bias) * alpha;

				vec3 V = normalize(-viewVec);
				vec3 N = worldNorm;

                float pdf;
                ImportanceSampleGGX(blueNoiseVec, N, V, biasedAlpha,
                                    ray.direction, pdf);

                if (pdf <= 0.0) {
					// What should we do here?
                    continue;
                }

                ray.inverseDirection = 1.0 / ray.direction;
                ray.origin = worldPos + ray.direction * EPSILON + worldNorm * EPSILON;

                ray.hitID = -1;
                ray.hitDistance = 0.0;

				vec3 radiance = vec3(0.0);

				if (material.roughness < 0.9) {
                	HitClosest(ray, 0.0, INF);
					radiance = EvaluateHit(ray);
				}
				else {
#ifdef GI
					radiance = GetLocalIrradiance(worldPos, V, N).rgb;
					radiance = IsInsideVolume(worldPos) ? radiance : vec3(0.0);
#endif
				}

				float radianceMax = max(max(radiance.r, 
						max(radiance.g, radiance.b)), radianceLimit);
				reflection += radiance * (radianceLimit / radianceMax);
            }

            reflection /= float(sampleCount);

        }

        imageStore(rtrImage, pixel, vec4(reflection, 1.0));
	}

}

vec3 EvaluateHit(inout Ray ray) {

	vec3 radiance = vec3(0.0);
	
	// If we didn't find a triangle along the ray,
	// we add the contribution of the environment map
	if (ray.hitID == -1) {
		return SampleEnvironmentMap(ray.direction).rgb;
	}
	
	// Unpack the compressed triangle and extract surface parameters
	Triangle tri = UnpackTriangle(triangles[ray.hitID]);
	bool backfaceHit;	
	Surface surface = GetSurfaceParameters(tri, ray, false, backfaceHit);
	
	radiance += surface.material.emissiveColor;

	// Evaluate direct light
	radiance += EvaluateDirectLight(surface);

	// Evaluate indirect lighting
#ifdef GI
	vec3 irradiance = GetLocalIrradiance(surface.P, surface.V, surface.N).rgb;
	// Approximate indirect specular for ray by using the irradiance grid
	// This enables metallic materials to have some kind of secondary reflection
	vec3 indirect = EvaluateIndirectDiffuseBRDF(surface) * irradiance +
		EvaluateIndirectSpecularBRDF(surface) * irradiance;
	radiance += IsInsideVolume(surface.P) ? indirect : vec3(0.0);
#endif

	return radiance;

}

vec3 EvaluateDirectLight(inout Surface surface) {

	if (GetLightCount() == 0)
		return vec3(0.0);

	float curSeed = float(frameSeed) / 255.0;
	float raySeed = float(gl_GlobalInvocationID.x);

	float lightPdf;
	Light light = GetLight(surface, raySeed, curSeed, lightPdf);

	float solidAngle, lightDistance;
	SampleLight(light, surface, raySeed, curSeed, solidAngle, lightDistance);

	// Evaluate the BRDF
	vec3 reflectance = EvaluateDiffuseBRDF(surface) + EvaluateSpecularBRDF(surface);
	reflectance *= surface.material.opacity;
	vec3 radiance = light.radiance * solidAngle;

	// Check for visibilty. This is important to get an
	// estimate of the solid angle of the light from point P
	// on the surface.
#ifdef USE_SHADOW_MAP
	radiance *= CalculateShadowWorldSpace(shadow, cascadeMaps, surface.P,
		surface.geometryNormal, saturate(dot(surface.L, surface.geometryNormal)));
#else
	radiance *= CheckVisibility(surface, lightDistance) ? 1.0 : 0.0;
#endif
	
	return reflectance * radiance * surface.NdotL / lightPdf;

}

bool CheckVisibility(Surface surface, float lightDistance) {

	if (surface.NdotL > 0.0) {
		Ray ray;
		ray.direction = surface.L;
		ray.origin = surface.P + surface.N * 2.0 * EPSILON;
		ray.inverseDirection = 1.0 / ray.direction;
		return HitAny(ray, 0.0, lightDistance - 4.0 * EPSILON) == false;
	}
	else {
		return false;
	}

}