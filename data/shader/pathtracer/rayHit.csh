#include <../raytracer/lights.hsh>
#include <../raytracer/tracing.hsh>
#include <../raytracer/direct.hsh>

#include <../common/random.hsh>
#include <../common/utility.hsh>
#include <../common/normalencode.hsh>
#include <../common/flatten.hsh>
#include <../common/PI.hsh>

#include <../brdf/brdfEval.hsh>
#include <../brdf/transmission.hsh>
#include <../brdf/brdfSample.hsh>
#include <../brdf/importanceSample.hsh>
#include <../brdf/surface.hsh>

layout (local_size_x = 32) in;

#ifdef REALTIME
layout (set = 3, binding = 1, r32ui) uniform uimage2DArray frameAccumImage;
layout (set = 3, binding = 5, rg16f) writeonly uniform image2D velocityImage;
layout (set = 3, binding = 6, r32f) writeonly uniform image2D depthImage;
layout (set = 3, binding = 7, rg16f) writeonly uniform image2D normalImage;
layout (set = 3, binding = 8, r16ui) writeonly uniform uimage2D materialIdxImage;
layout (set = 3, binding = 9, rgba8) writeonly uniform image2D albedoImage;
#else
layout (set = 3, binding = 1, rgba8) writeonly uniform image2D outputImage;
#endif

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
    int samplesPerFrame;
    float maxRadiance;
} Uniforms;

const float gamma = 1.0 / 2.2;

Surface EvaluateBounce(inout Ray ray, inout RayPayload payload);
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
        
        ivec2 pixel = Unflatten2D(ray.ID / Uniforms.samplesPerFrame, Uniforms.resolution);
        
        Surface surface = EvaluateBounce(ray, payload);

        vec4 accumColor = vec4(0.0);

        float energy = dot(payload.throughput, vec3(1.0));

#ifdef REALTIME
        // Write out material information and velocity into a g-buffer
        if (ray.ID % Uniforms.samplesPerFrame == 0 && Uniforms.bounceCount == 0) {
            Instance instance;
            mat4 lastMatrix;
            vec3 position;
            uint materialId;
            vec3 geometryNormal;
            if (ray.hitID >= 0) {
                instance = bvhInstances[nonuniformEXT(ray.hitInstanceID)];
                lastMatrix = mat4(transpose(instanceLastMatrices[nonuniformEXT(ray.hitInstanceID)]));

                position = surface.P;
                materialId = surface.material.ID;
                geometryNormal = surface.geometryNormal;
            }
            else {
                instance.inverseMatrix = mat3x4(1.0);
                lastMatrix = mat4(1.0);
                position = ray.origin + ray.direction * 100.0;
                materialId = 65535;
                geometryNormal = vec3(1.0, 0.0, 0.0);
            }

            // As a reminder: The current inverse matrix is also transposed
            vec3 lastPos = vec3(lastMatrix * vec4(vec4(position, 1.0) * instance.inverseMatrix, 1.0));

            vec4 viewSpacePos = globalData.vMatrix * vec4(position, 1.0);
            vec4 projPositionCurrent = globalData.pMatrix * viewSpacePos;
            vec4 projPositionLast = globalData.pMatrix * globalData.vMatrixLast * vec4(lastPos, 1.0);

            vec2 ndcCurrent = projPositionCurrent.xy / projPositionCurrent.w;
            vec2 ndcLast = projPositionLast.xy / projPositionLast.w;

            vec2 velocity = (ndcLast - ndcCurrent) * 0.5;
            
            imageStore(velocityImage, pixel, vec4(velocity, 0.0, 0.0));

            imageStore(depthImage, pixel, vec4(ray.hitID >= 0 ? projPositionCurrent.z / projPositionCurrent.w : INF, 0.0, 0.0, 0.0));
            imageStore(materialIdxImage, pixel, uvec4(materialId, 0.0, 0.0, 0.0));
            imageStore(normalImage, pixel, vec4(EncodeNormal(geometryNormal), 0.0, 0.0));            
        }
#endif

        if (energy == 0 || Uniforms.bounceCount == Uniforms.maxBounces) {
#ifndef REALTIME
            if (Uniforms.sampleCount > 0)
                accumColor = imageLoad(inAccumImage, pixel);

            accumColor += vec4(payload.radiance, 1.0);
            imageStore(outAccumImage, pixel, accumColor);

            vec3 color = accumColor.rgb * Uniforms.exposure / float(Uniforms.sampleCount + 1);
            color = vec3(1.0) - exp(-color);
            //color = color / (vec3(1.0) + color);

            imageStore(outputImage, pixel,
                vec4(pow(color, vec3(gamma)), 1.0));
#else
            if (Uniforms.samplesPerFrame == 1) {
                imageStore(frameAccumImage, ivec3(pixel, 0), uvec4(floatBitsToUint(payload.radiance.r)));
                imageStore(frameAccumImage, ivec3(pixel, 1), uvec4(floatBitsToUint(payload.radiance.g)));
                imageStore(frameAccumImage, ivec3(pixel, 2), uvec4(floatBitsToUint(payload.radiance.b)));
            }
            else {
#ifndef AE_OS_MACOS
                uint maxValuePerSample = 0xFFFFFFFF / uint(Uniforms.samplesPerFrame);

                vec3 normalizedRadiance = clamp(payload.radiance / Uniforms.maxRadiance, vec3(0.0), vec3(1.0));
                uvec3 quantizedRadiance = clamp(uvec3(normalizedRadiance * float(maxValuePerSample)), uvec3(0u), uvec3(maxValuePerSample));

                imageAtomicAdd(frameAccumImage, ivec3(pixel, 0), quantizedRadiance.r);
                imageAtomicAdd(frameAccumImage, ivec3(pixel, 1), quantizedRadiance.g);
                imageAtomicAdd(frameAccumImage, ivec3(pixel, 2), quantizedRadiance.b);
#endif
            }
#endif
        }
        else {
            WriteRay(ray, payload);
        }
    }

}

Surface EvaluateBounce(inout Ray ray, inout RayPayload payload) {
    
    Surface surface;
    
    // If we didn't find a triangle along the ray,
    // we add the contribution of the environment map
    if (ray.hitID == -1) {
        // Clamp env map, since there is only a uniform sampling for now
        payload.radiance += min(SampleEnvironmentMap(ray.direction).rgb *
            payload.throughput, vec3(1000.0));
        payload.throughput = vec3(0.0);
        return surface;    
    }

    int lod = int(ray.hitDistance / 30.0);
    lod = 0;
    
    // Unpack the compressed triangle and extract surface parameters
    Instance instance = GetInstance(ray);
    Triangle tri = GetTriangle(ray, instance);
    surface = GetSurfaceParameters(instance, tri, ray, true, lod);

    // If we hit an emissive surface we need to terminate the ray
#ifndef REALTIME
    if (dot(surface.material.emissiveColor, vec3(1.0)) > 0.0 &&
        Uniforms.bounceCount == 0) {
        payload.radiance += surface.material.emissiveColor;
    }
#else
    payload.radiance += payload.throughput * surface.material.emissiveColor;
#endif

    // Evaluate direct and indirect light
    vec3 radiance = payload.throughput * saturate(surface.material.opacity)
        * EvaluateDirectLight(surface);
    
    // Clamp indirect radiance
    // This has to be used cautiously and might be the reason
    // for some artifacting happening (e.g. Sponza arches)
    // When the propability of an indirect sample is low,
    // the contribution might be reduced by this and the lighting
    // will be heavily biased or not there entirely. Better lobe
    // selection etc. helps
    if (Uniforms.bounceCount > 0) {
        const float radianceLimit = 10.0;
        float radianceMax = max(max(radiance.r, 
            max(radiance.g, radiance.b)), radianceLimit);
        radiance *= radianceLimit / radianceMax;
    }

    payload.radiance += radiance;
    EvaluateIndirectLight(surface, ray, payload);
    return surface;

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

    if (lightPdf <= 0.0) return vec3(0.0);

    vec3 transmissionColor = GetTransmissionColor(surface.material);
    // Front side reflection, transmission is evaluated separately below
    vec3 reflectance = EvaluateDiffuseBRDF(surface) + EvaluateSpecularBRDF(surface);
    reflectance *= surface.material.opacity;
    vec3 radiance = light.radiance * solidAngle;
    float visibility = light.castShadow ? CheckVisibility(surface, lightDistance) : 1.0;
    vec3 direct = reflectance * radiance * surface.NdotL * visibility / lightPdf;

    if (surface.material.transmissive && dot(surface.L, surface.geometryNormal) < 0.0) {
        Surface backSurface = GetTransmissionSurface(surface);
        if (backSurface.NdotL > 0.0) {
            float backVisibility = light.castShadow ? CheckVisibility(backSurface, lightDistance) : 1.0;
            direct += transmissionColor * EvaluateDiffuseBRDF(backSurface) * surface.material.opacity *
                radiance * backSurface.NdotL * backVisibility / lightPdf;
        }
    }

    return direct;

}

void EvaluateIndirectLight(inout Surface surface, inout Ray ray, inout RayPayload payload) {

    Material mat = surface.material;
    float opacity = saturate(mat.opacity);
    vec3 transmissionColor = GetTransmissionColor(mat);

    float curSeed = Uniforms.seed;
    float raySeed = float(ray.ID);

    float passThroughChance = opacity == 1.0 ? 0.0 : clamp(1.0 - opacity, 0.1, 0.9);

    float rnd = saturate(random(raySeed, curSeed));

    BRDFSample brdfSample;

    bool transmitted = false;
    bool passedThrough = false;
    float cosine = 1.0;
    if (rnd >= passThroughChance) {
        rnd = random(raySeed, curSeed);

        vec3 F = FresnelSchlick(surface.F0, surface.F90, surface.NdotV);
        // Reduces variance
        float specChance = clamp(dot(F, vec3(0.33333)), 0.1, 0.9);

        float u0 = random(raySeed, curSeed);
        float u1 = random(raySeed, curSeed);

        if (rnd < specChance) {
            brdfSample = SampleSpecularBRDF(surface, vec2(u0, u1));
            brdfSample.reflectance *= opacity;
            brdfSample.pdf *= specChance;
            cosine = surface.NdotL;
        }
        else {
            // Transmission is an additional diffuse lobe on the back side, choose between both lobes
            float transmissionWeight = dot(transmissionColor, vec3(1.0 / 3.0));
            float transmissionChance = transmissionWeight / (1.0 + transmissionWeight);
            if (mat.metalness >= 1.0) transmissionChance = 0.0;
            if (transmissionChance > 0.0 && random(raySeed, curSeed) < transmissionChance) {
                Surface backSurface = GetTransmissionSurface(surface);
                brdfSample = SampleDiffuseBRDF(backSurface, vec2(u0, u1));
                brdfSample.reflectance *= (1.0 - mat.metalness) * transmissionColor * opacity;
                brdfSample.pdf *= (1.0 - specChance) * transmissionChance;
                cosine = backSurface.NdotL;
                transmitted = true;
            }
            else {
                brdfSample = SampleDiffuseBRDF(surface, vec2(u0, u1));
                brdfSample.reflectance *= (1.0 - mat.metalness) * opacity;
                brdfSample.pdf *= (1.0 - specChance) * (1.0 - transmissionChance);
                cosine = surface.NdotL;
            }
        }

        brdfSample.pdf *= (1.0 - passThroughChance);
    }
    else {
        brdfSample.L = ray.direction;
        brdfSample.pdf = passThroughChance;
        brdfSample.reflectance = vec3(1.0 - opacity);
        transmitted = true;
        passedThrough = true;
    }

    if (brdfSample.pdf > 0.0 && sum(brdfSample.reflectance) > 0.0) {
        payload.throughput *= brdfSample.reflectance *
            cosine / brdfSample.pdf;
    }
    else {
        payload.throughput = vec3(0.0);
        return;
    }
    
    ray.direction = normalize(brdfSample.L);
    float geometryCosine = dot(ray.direction, surface.geometryNormal);
    if (!passedThrough && (transmitted ? geometryCosine >= 0.0 : geometryCosine <= 0.0)) {
        payload.throughput = vec3(0.0);
        return;
    }
    // Offset the origin to the side the ray continues on
    ray.origin = transmitted && !passedThrough ? surface.P - surface.geometryNormal * EPSILON :
        passedThrough ? surface.P - surface.N * EPSILON : surface.P + surface.V * EPSILON;
    payload.throughput *= mat.ao;

    // Russain roulette, terminate rays with a chance of one percent
    float probability = clamp(max(payload.throughput.r,
        max(payload.throughput.g, payload.throughput.b)), 0.01, 0.99);

    probability = Uniforms.bounceCount < 3 ? min(3.0 * probability, 1.0) : probability;

    if (random(raySeed, curSeed) > probability) {
        payload.throughput = vec3(0.0);
        return;
    }

    payload.throughput /= probability;

}

float CheckVisibility(Surface surface, float lightDistance) {

    if (surface.NdotL > 0.0) {
        Ray ray;
        ray.direction = surface.L;
        // For transmitted light N is already flipped to the back side
        ray.origin = surface.P + surface.N * EPSILON;
        return HitAnyTransparency(ray, INSTANCE_MASK_SHADOW, 0.0, lightDistance - 2.0 * EPSILON);
    }
    else {
        return 0.0;
    }

}
