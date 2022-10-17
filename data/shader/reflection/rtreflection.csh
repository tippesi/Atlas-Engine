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

layout (local_size_x = 8, local_size_y = 4) in;

layout (binding = 4, rgba16f) writeonly uniform image2D rtrImage;

layout(binding = 16) uniform sampler2D normalTexture;
layout(binding = 17) uniform sampler2D depthTexture;
layout(binding = 18) uniform sampler2D roughnessMetallicAoTexture;
layout(binding = 19) uniform isampler2D offsetTexture;
layout(binding = 20) uniform usampler2D materialIdxTexture;
layout(binding = 21) uniform sampler2D randomTexture;

const ivec2 offsets[4] = ivec2[4](
    ivec2(0, 0),
    ivec2(1, 0),
    ivec2(0, 1),
    ivec2(1, 1)
);

uniform uint sampleCount;
uniform float radius;

uniform mat4 pMatrix;
uniform mat4 ivMatrix;

uniform ivec2 resolution;
uniform float frameSeed;

vec3 EvaluateHit(inout Ray ray);
vec3 EvaluateDirectLight(Surface surface);
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
        vec3 randomVec = normalize(vec3(2.0 * texelFetch(randomTexture, pixel % ivec2(2), 0).xy - 1.0, 0.0));

        uint materialIdx = texelFetch(materialIdxTexture, pixel * 2 + offset, 0).r;
	    Material material = UnpackMaterial(materialIdx);

        float roughness = texelFetch(roughnessMetallicAoTexture, pixel, 0).r;
        material.roughness *= material.roughnessMap ? roughness : 1.0;

        vec3 reflection = vec3(0.0);

        if (material.roughness < 0.9) {
            const int sampleCount = 1;

            float raySeed = float(pixel.x + pixel.y * resolution.x) * 2 * float(sampleCount);
            float curSeed = float(frameSeed);

            for (uint i = 0; i < sampleCount; i++) {
                Ray ray;

                float u0 = random(raySeed, curSeed);
                float u1 = random(raySeed, curSeed);

                float pdf;
                ImportanceSampleGGX(vec2(u0, u1), worldNorm, normalize(-viewVec), sqr(material.roughness),
                                    ray.direction, pdf);

                //if (pdf == 0.0) continue;

                ray.inverseDirection = 1.0 / ray.direction;
                ray.origin = worldPos + ray.direction * EPSILON + worldNorm * EPSILON;

                ray.hitID = -1;
                ray.hitDistance = 0.0;

                HitClosest(ray, 0.0, INF);

                reflection += min(EvaluateHit(ray), vec3(1.0));
            }

            reflection /= float(sampleCount);

        }

        imageStore(rtrImage, pixel, vec4(reflection, 0.0));
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

	// Need to sample the volume later for infinite bounces:
	//vec3 indirect = EvaluateIndirectDiffuseBRDF(surface) *
	//	GetLocalIrradiance(surface.P, surface.V, surface.N).rgb;
	//radiance += IsInsideVolume(surface.P) ? indirect : vec3(0.0);
	return radiance;

}

vec3 EvaluateDirectLight(Surface surface) {

	if (GetLightCount() == 0)
		return vec3(0.0);

	float curSeed = frameSeed;
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
	if (CheckVisibility(surface, lightDistance) == false)
		radiance = vec3(0.0);
	
	return reflectance * radiance * surface.NdotL / lightPdf;

}

bool CheckVisibility(Surface surface, float lightDistance) {

	if (surface.NdotL > 0.0) {
		Ray ray;
		ray.direction = surface.L;
		ray.origin = surface.P + surface.N * EPSILON;
		ray.inverseDirection = 1.0 / ray.direction;
		return HitAny(ray, 0.0, lightDistance - 2.0 * EPSILON) == false;
	}
	else {
		return false;
	}

}