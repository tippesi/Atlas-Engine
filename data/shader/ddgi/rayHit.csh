// Based on DDGI: 
// Majercik, Zander, et al. "Dynamic diffuse global illumination with ray-traced irradiance fields."
// Journal of Computer Graphics Techniques Vol 8.2 (2019).
// Majercik, Zander, et al. "Scaling Probe-Based Real-Time Dynamic Global Illumination for Production."
// arXiv preprint arXiv:2009.10796 (2020).

#include <../raytracer/lights.hsh>
#include <../raytracer/tracing.hsh>
#include <../raytracer/direct.hsh>

#include <../common/random.hsh>
#include <../common/utility.hsh>
#include <../common/flatten.hsh>
#include <../common/PI.hsh>

#include <../brdf/brdfEval.hsh>
#include <../brdf/importanceSample.hsh>
#include <../brdf/surface.hsh>

#include <ddgi.hsh>

layout (local_size_x = 32) in;

// Instead of write ray array use hits array
layout(std430, binding = 3) buffer RayHits {
	PackedRayHit hits[];
};

uniform float seed;

vec3 EvaluateHit(inout Ray ray);
vec3 EvaluateDirectLight(Surface surface);
bool CheckVisibility(Surface surface, float lightDistance);

void main() {
	
	if (IsRayInvocationValid()) {	
		Ray ray = ReadRay();
		
		vec3 radiance = EvaluateHit(ray);
		
		RayHit hit;
		hit.radiance = radiance;
		hit.direction = ray.direction;
		hit.hitDistance = ray.hitDistance;

		hits[ray.ID] = PackRayHit(hit);
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

	// Indicates backface hit
	if (backfaceHit) {
		ray.hitDistance = -ray.hitDistance;
		return vec3(0.0);
	}
	
	radiance += surface.material.emissiveColor;

	// Evaluate direct light
	radiance += EvaluateDirectLight(surface);

	// Need to sample the volume later for infinite bounces:
	vec3 indirect = EvaluateIndirectDiffuseBRDF(surface) *
		GetLocalIrradiance(surface.P, surface.V, surface.N).rgb;
	radiance += IsInsideVolume(surface.P) ? indirect : vec3(0.0);

	return radiance;

}

vec3 EvaluateDirectLight(Surface surface) {

	if (GetLightCount() == 0)
		return vec3(0.0);

	float curSeed = seed;
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