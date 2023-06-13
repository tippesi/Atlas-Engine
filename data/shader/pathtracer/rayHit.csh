#include <../raytracer/lights.hsh>
#include <../raytracer/tracing.hsh>
#include <../raytracer/direct.hsh>

#include <../common/random.hsh>
#include <../common/utility.hsh>
#include <../common/flatten.hsh>
#include <../common/PI.hsh>

#include <../brdf/brdfEval.hsh>
#include <../brdf/brdfSample.hsh>
#include <../brdf/importanceSample.hsh>
#include <../brdf/surface.hsh>

layout (local_size_x = 32) in;

layout (set = 3, binding = 1, rgba8) writeonly uniform image2D outputImage;

/*
Except for image variables qualified with the format qualifiers r32f, r32i, and r32ui, 
image variables must specify either memory qualifier readonly or the memory qualifier writeonly.
Reading and writing simultaneously to other formats is not supported on OpenGL ES
*/
layout (set = 3, binding = 2, rgba32f) readonly uniform image2D inAccumImage;
layout (set = 3, binding = 3, rgba32f) writeonly uniform image2D outAccumImage;

layout(set = 3, binding = 4) uniform UniformBuffer {
    ivec2 resolution;
    int maxBounces;
    int sampleCount;
    int bounceCount;
    float seed;
    float exposure;
} Uniforms;

const float gamma = 1.0 / 2.2;

void EvaluateBounce(inout Ray ray, inout RayPayload payload);
vec3 EvaluateDirectLight(inout Surface surface);
void EvaluateIndirectLight(inout Surface surface, inout Ray ray, inout RayPayload payload);
float CheckVisibility(Surface surface, float lightDistance);

void main() {
    
    if (IsRayInvocationValid()) {
        Ray ray = ReadRay();
        RayPayload payload;

        payload.radiance = vec3(0.0);
        payload.throughput = vec3(1.0);

        // Read payload when not in first bounce
        if (Uniforms.bounceCount > 0)
            payload = ReadRayPayload();
        
        ivec2 pixel = Unflatten2D(ray.ID, Uniforms.resolution);
        
        EvaluateBounce(ray, payload);
        
        vec4 accumColor = vec4(0.0);

        float energy = dot(payload.throughput, vec3(1.0));
        
        if (energy == 0 || Uniforms.bounceCount == Uniforms.maxBounces) {            
            if (Uniforms.sampleCount > 0)
                accumColor = imageLoad(inAccumImage, pixel);
            
            accumColor += vec4(payload.radiance, 1.0);            
            imageStore(outAccumImage, pixel, accumColor);
            
            vec3 color = accumColor.rgb * Uniforms.exposure / float(Uniforms.sampleCount + 1);
            color = vec3(1.0) - exp(-color);
            //color = color / (vec3(1.0) + color);

            imageStore(outputImage, pixel,
                vec4(pow(color, vec3(gamma)), 1.0));
        }
        else {
            WriteRay(ray, payload);
        }
    }

}

void EvaluateBounce(inout Ray ray, inout RayPayload payload) {
    
    // If we didn't find a triangle along the ray,
    // we add the contribution of the environment map
    if (ray.hitID == -1) {
        // Clamp env map, since there is only a uniform sampling for now
        payload.radiance += min(SampleEnvironmentMap(ray.direction).rgb * 
            payload.throughput, vec3(10.0));
        payload.throughput = vec3(0.0);
        return;    
    }
    
    // Unpack the compressed triangle and extract surface parameters
    Triangle tri = UnpackTriangle(triangles[ray.hitID]);
    Surface surface = GetSurfaceParameters(tri, ray, true, 0);

    // If we hit an emissive surface we need to terminate the ray
    if (dot(surface.material.emissiveColor, vec3(1.0)) > 0.0 &&
        Uniforms.bounceCount == 0) {
        payload.radiance += surface.material.emissiveColor;
    }

    // Evaluate direct and indirect light
    vec3 radiance = payload.throughput * surface.material.opacity
        * EvaluateDirectLight(surface);
    
    // Clamp indirect radiance
    // This has to be used cautiously and might be the reason
    // for some artifacting happening (e.g. Sponza arches)
    // When the propability of an indirect sample is low,
    // the contribution might be reduced by this and the lighting
    // will be heavily biased or not there entirely. Better lobe
    // selection etc. helps
    if (Uniforms.bounceCount > 0) {
        const float radianceLimit = 100.0;
        float radianceMax = max(max(radiance.r, 
            max(radiance.g, radiance.b)), radianceLimit);
        radiance *= radianceLimit / radianceMax;
    }

    payload.radiance += radiance;
    EvaluateIndirectLight(surface, ray, payload);
    return;

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
    
    // Evaluate the BRDF
    vec3 reflectance = EvaluateDiffuseBRDF(surface) + EvaluateSpecularBRDF(surface);
    reflectance *= surface.material.opacity;
    vec3 radiance = light.radiance * solidAngle;

    // Check for visibilty. This is important to get an
    // estimate of the solid angle of the light from point P
    // on the surface.
    radiance *= CheckVisibility(surface, lightDistance);
    
    return reflectance * radiance * surface.NdotL / lightPdf;

}

void EvaluateIndirectLight(inout Surface surface, inout Ray ray, inout RayPayload payload) {

    // Something that is not ideal right now:
    // There should be a difference between the shading normal and the 
    // geometry normal. Hemisphere sampling should either be done using the geometry
    // normal or we should check if we somehow sampled the lower hemisphere below
    // the surface. In that case we need to stop the ray to prevent leaking from below.
    // This case can only happen with normal maps enabled. Additionally we should use
    // the previous ray direction/view direction to offset the ray origin from the surface
    Material mat = surface.material;
    ray.origin = surface.P;

    float curSeed = Uniforms.seed;
    float raySeed = float(ray.ID);

    float refractChance = clamp(1.0 - mat.opacity, 0.1, 0.9);
    refractChance = mat.opacity == 1.0 ? 0.0 : refractChance;

    float rnd = saturate(random(raySeed, curSeed));

    BRDFSample brdfSample;

    bool refracted = false;
    if (rnd >= refractChance) {
        rnd = random(raySeed, curSeed);

        vec3 F = FresnelSchlick(surface.F0, surface.F90, surface.NdotV);
        // Reduces variance
        float specChance = clamp(dot(F, vec3(0.33333)), 0.1, 0.9);

        float u0 = random(raySeed, curSeed);
        float u1 = random(raySeed, curSeed);

        if (rnd < specChance) {
            brdfSample = SampleSpecularBRDF(surface, vec2(u0, u1));
            brdfSample.reflectance *= mat.opacity;
            brdfSample.pdf *= specChance;
        }
        else {
            brdfSample = SampleDiffuseBRDF(surface, vec2(u0, u1));
            brdfSample.reflectance *= (1.0 - surface.material.metalness) * mat.opacity;
            brdfSample.pdf *= (1.0 - specChance);
        }

        brdfSample.pdf *= (1.0 - refractChance);
        ray.origin += surface.V * EPSILON;
    }
    else {
        brdfSample.L = ray.direction;
        brdfSample.pdf = refractChance;
        brdfSample.reflectance = vec3(1.0 - mat.opacity);
        
        // We want full throughput
        surface.NdotL = 1.0;
        ray.origin -= surface.N * EPSILON;

        refracted = true;
    }

    if (brdfSample.pdf > 0.0 && sum(brdfSample.reflectance) > 0.0) {
        payload.throughput *= brdfSample.reflectance *
            surface.NdotL / brdfSample.pdf;
    }
    else {
        payload.throughput = vec3(0.0);
    }
    
    ray.direction = normalize(brdfSample.L);
    ray.inverseDirection = 1.0 / ray.direction;
    payload.throughput *= mat.ao;

    // Russain roulette, terminate rays with a chance of one percent
    float probability = clamp(max(payload.throughput.r,
        max(payload.throughput.g, payload.throughput.b)), 0.01, 0.99);

    if (random(raySeed, curSeed) > probability) {
        payload.throughput = vec3(0.0);
        return;
    }

    if (dot(ray.direction, surface.geometryNormal) <= 0.0 && !refracted) {
        payload.throughput = vec3(0.0);
        return;
    }

    payload.throughput /= probability;

}

float CheckVisibility(Surface surface, float lightDistance) {

    if (surface.NdotL > 0.0) {
        Ray ray;
        ray.direction = surface.L;
        ray.origin = surface.P + surface.N * EPSILON;
        ray.inverseDirection = 1.0 / ray.direction;
        return HitAnyShadow(ray, 0.0, lightDistance - 2.0 * EPSILON);
    }
    else {
        return 0.0;
    }

}