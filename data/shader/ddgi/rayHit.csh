// Based on DDGI: 
// Majercik, Zander, et al. "Dynamic diffuse global illumination with ray-traced irradiance fields."
// Journal of Computer Graphics Techniques Vol 8.2 (2019).
// Majercik, Zander, et al. "Scaling Probe-Based Real-Time Dynamic Global Illumination for Production."
// arXiv preprint arXiv:2009.10796 (2020).
#define LOCAL_STACK

#include <../raytracer/lights.hsh>
#include <../raytracer/tracing.hsh>
#include <../raytracer/direct.hsh>

#include <../common/random.hsh>
#include <../common/utility.hsh>
#include <../common/flatten.hsh>
#include <../common/PI.hsh>

#include <../brdf/brdfEval.hsh>
#include <../brdf/transmission.hsh>
#include <../brdf/importanceSample.hsh>
#include <../brdf/surface.hsh>

#include <../clouds/shadow.hsh>
#include <ddgi.hsh>
#include <transmission.hsh>
#include <shadowRay.hsh>

layout (local_size_x = 32) in;

#ifdef CLOUD_SHADOWS
layout(set = 3, binding = 1) uniform sampler2D cloudMap;
#endif

// Instead of write ray array use hits array
layout(std430, set = 3, binding = 2) buffer RayHits {
    PackedRayHit hits[];
};

#ifdef DEFERRED_SHADOW_RAYS
layout(std430, set = 3, binding = 4) buffer ShadowRays {
    PackedShadowRay shadowRays[];
};

layout(std430, set = 3, binding = 5) buffer ShadowRayCount {
    uint shadowRayCount;
};
#endif

layout(std140, set = 3, binding = 3) uniform UniformBuffer {
    float seed;
} Uniforms;

vec3 EvaluateHit(inout Ray ray);
vec3 EvaluateDirectLight(inout Surface surface);
bool CheckVisibility(Surface surface, float lightDistance);

bool hasShadowRay = false;
PackedShadowRay shadowRay;

void main() {
    
    if (IsRayInvocationValid()) {    
        Ray ray = ReadRay();
        
        vec3 radiance = vec3(0.0);
        if (ray.ID >= 0) {
            radiance = EvaluateHit(ray);

            // Probes store the lighting scaled, the lookup restores it
            radiance *= ddgiData.volumeStorageScale;

#ifdef DEFERRED_SHADOW_RAYS
            if (hasShadowRay) {
                uint shadowRayIdx = atomicAdd(shadowRayCount, 1u);
                shadowRays[shadowRayIdx].origin = shadowRay.origin;
                shadowRays[shadowRayIdx].direction = shadowRay.direction;
                shadowRays[shadowRayIdx].radiance = vec4(shadowRay.radiance.rgb * ddgiData.volumeStorageScale, uintBitsToFloat(uint(ray.ID)));
            }
#endif
        
            RayHit hit;
#ifndef AE_HALF_FLOAT
            hit.radiance.rgb = radiance;
            hit.direction.xyz = ray.direction;
            hit.radiance.a = ray.hitDistance;
#else
            hit.radiance = AeF16x4(radiance, ray.hitDistance);
            hit.direction = AeF16x4(ray.direction, 1.0);
#endif

            hits[ray.ID] = PackRayHit(hit);
        }
    }

}

vec3 EvaluateHit(inout Ray ray) {

    vec3 radiance = vec3(0.0);
    
    // If we didn't find a triangle along the ray,
    // we add the contribution of the environment map
    if (ray.hitID == -1) {
        return SampleEnvironmentMap(ray.direction, 4).rgb;
    }
    
    // Unpack the compressed triangle and extract surface parameters
    Instance instance = GetInstance(ray);
    Triangle tri = GetTriangle(ray, instance);

    bool backfaceHit;
    Surface surface = GetSurfaceParameters(instance, tri, ray, false, backfaceHit, 4);

    // Indicates backface hit
    if (backfaceHit) {
        ray.hitDistance = -ray.hitDistance;
        return vec3(0.0);
    }
    
    radiance += surface.material.emissiveColor;

    // Evaluate direct light
    vec3 direct = EvaluateDirectLight(surface);
    if (hasShadowRay)
        shadowRay.radiance = vec4(direct, 0.0);
    else
        radiance += direct;

    // Without RADIANCE_VOLUME the probe radiance falls back to the irradiance, the large radiance
    // probes are too expensive to sample at incoherent hits for the small specular part of the bounce
    vec4 probeIrradiance, probeRadiance;
    GetLocalProbeLighting(surface.P, surface.V, surface.N, surface.N, surface.geometryNormal, probeIrradiance, probeRadiance);

    vec3 indirect = EvaluateIndirectDiffuseBRDF(surface) * probeIrradiance.rgb +
        EvaluateIndirectSpecularBRDF(surface) * probeRadiance.rgb;
    indirect += GetTransmittedIndirectDiffuse(surface);

    // Need to sample the volume later for infinite bounces:
    //vec3 indirect = EvaluateIndirectDiffuseBRDF(surface) *
    //    GetLocalIrradiance(surface.P, surface.V, surface.N).rgb;
    
    //indirect += EvaluateIndirectSpecularBRDF(surface) * 
    //    GetLocalRadiance(surface.P, surface.V, surface.N).rgb;

    radiance += IsInsideVolume(surface.P) ? indirect : vec3(0.0);
    return radiance;

}

vec3 EvaluateDirectLight(inout Surface surface) {

    if (GetLightCount() == 0)
        return vec3(0.0);

    float curSeed = Uniforms.seed;
    float raySeed = float(gl_GlobalInvocationID.x);

    float lightPdf;
    Light light = GetLight(surface, raySeed, curSeed, lightPdf);

    float solidAngle, lightDistance;
    SampleLight(light, surface, raySeed, curSeed, solidAngle, lightDistance);

    bool transmittedLight = IsTransmittedLight(surface);
    Surface lightSurface = transmittedLight ? GetTransmissionSurface(surface) : surface;
    
    // Evaluate the BRDF
    vec3 reflectance = EvaluateDiffuseBRDF(lightSurface);
    if (transmittedLight) reflectance *= GetTransmissionColor(surface.material);
    reflectance *= surface.material.opacity;
    vec3 radiance = light.radiance * solidAngle;

    // Check for visibilty. This is important to get an
    // estimate of the solid angle of the light from point P
    // on the surface.
    if (light.castShadow)
        radiance *= CheckVisibility(lightSurface, lightDistance) ? 1.0 : 0.0;

#ifdef CLOUD_SHADOWS
    if (light.type == uint(DIRECTIONAL_LIGHT)) {
        vec3 P = vec3(globalData.vMatrix * vec4(surface.P, 1.0));
        float cloudShadowFactor = CalculateCloudShadow(P, cloudShadowUniforms.cloudShadow, cloudMap);

        radiance *= cloudShadowFactor;
    }
#endif
    
    return reflectance * radiance * lightSurface.NdotL / lightPdf;

}

bool CheckVisibility(Surface surface, float lightDistance) {

    if (surface.NdotL > 0.0) {
        Ray ray;
        ray.direction = surface.L;
        float side = dot(surface.L, surface.geometryNormal) < 0.0 ? -1.0 : 1.0;
        ray.origin = surface.P + surface.geometryNormal * (side * EPSILON);
#ifdef DEFERRED_SHADOW_RAYS
        hasShadowRay = true;
        shadowRay.origin = vec4(ray.origin, lightDistance - 2.0 * EPSILON);
        shadowRay.direction = vec4(ray.direction, 0.0);
        return true;
#else
        return HitAny(ray, INSTANCE_MASK_SHADOW, 0.0, lightDistance - 2.0 * EPSILON) == false;
#endif
    }
    else {
        return false;
    }

}
