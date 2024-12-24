#define SHADOW_FILTER_1x1

#include <../globals.hsh>
#include <../raytracer/lights.hsh>
#include <../raytracer/tracing.hsh>
#include <../raytracer/direct.hsh>

#include <../common/random.hsh>
#include <../common/utility.hsh>
#include <../common/flatten.hsh>
#include <../common/convert.hsh>
#include <../common/normalencode.hsh>
#include <../common/PI.hsh>
#include <../common/bluenoise.hsh>

#include <../brdf/brdfEval.hsh>
#include <../brdf/brdfSample.hsh>
#include <../brdf/importanceSample.hsh>
#include <../brdf/surface.hsh>

#include <../ddgi/ddgi.hsh>
#include <../shadow.hsh>
#include <../clouds/shadow.hsh>

layout (local_size_x = 8, local_size_y = 4) in;

layout (set = 3, binding = 0, rgba16f) uniform image2D rtrImage;

layout(set = 3, binding = 1) uniform sampler2D normalTexture;
layout(set = 3, binding = 2) uniform sampler2D depthTexture;
layout(set = 3, binding = 3) uniform sampler2D roughnessMetallicAoTexture;
layout(set = 3, binding = 4) uniform isampler2D offsetTexture;
layout(set = 3, binding = 5) uniform usampler2D materialIdxTexture;
layout(set = 3, binding = 6) uniform sampler2DArrayShadow cascadeMaps;

layout(set = 3, binding = 7) uniform sampler2D scramblingRankingTexture;
layout(set = 3, binding = 8) uniform sampler2D sobolSequenceTexture;

#ifdef CLOUD_SHADOWS
layout(set = 3, binding = 9) uniform sampler2D cloudMap;
#endif

#ifdef AUTO_EXPOSURE
layout(set = 3, binding = 11) uniform sampler2D exposureTexture;
#endif

const ivec2 offsets[4] = ivec2[4](
    ivec2(0, 0),
    ivec2(1, 0),
    ivec2(0, 1),
    ivec2(1, 1)
);

layout(std140, set = 3, binding = 10) uniform UniformBuffer {
    float radianceLimit;
    uint frameSeed;
    float bias;
    int sampleCount;
    int lightSampleCount;
    int textureLevel;
    float roughnessCutoff;
    int halfRes;
    ivec2 resolution;
    int padding0;
    int padding1;
    Shadow shadow;
} uniforms;

vec3 EvaluateHit(inout Ray ray);
vec3 EvaluateDirectLight(inout Surface surface, inout float seed);
float CheckVisibility(Surface surface, float lightDistance);

void main() {

    ivec2 resolution = uniforms.resolution;

    if (int(gl_GlobalInvocationID.x) < resolution.x &&
        int(gl_GlobalInvocationID.y) < resolution.y) {

        ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

#ifdef SSR
        vec4 reflection = imageLoad(rtrImage, pixel);
#else
        vec4 reflection = vec4(0.0);
#endif

        // No need, there is no offset right now
        int offsetIdx = texelFetch(offsetTexture, pixel, 0).r;
        ivec2 offset = offsets[offsetIdx];

        float depth = texelFetch(depthTexture, pixel, 0).r;

        ivec2 highResPixel;
        vec2 recontructTexCoord;
        if (uniforms.halfRes > 0) {
            recontructTexCoord = (2.0 * (vec2(pixel)) + offset + 0.5) / (2.0 * vec2(resolution));
            highResPixel = 2 * pixel + offset;
        }
        else {
            recontructTexCoord = (vec2(pixel) + 0.5) / vec2(resolution);
            highResPixel = pixel;
        }
            
        vec3 viewPos = ConvertDepthToViewSpace(depth, recontructTexCoord);
        vec3 worldPos = vec3(globalData.ivMatrix * vec4(viewPos, 1.0));
        vec3 viewVec = vec3(globalData.ivMatrix * vec4(viewPos, 0.0));
        vec3 worldNorm = normalize(vec3(globalData.ivMatrix * 
            vec4(DecodeNormal(texelFetch(normalTexture, pixel, 0).rg), 0.0)));

        uint materialIdx = texelFetch(materialIdxTexture, pixel, 0).r;
        Material material = UnpackMaterial(materialIdx);

        float roughness = texelFetch(roughnessMetallicAoTexture, pixel, 0).r;
        material.roughness *= material.roughnessMap || material.terrain ? roughness : 1.0;

        float hitDistance = reflection.a;

        if (material.roughness <= 1.0 && depth < 1.0 && reflection.a == 0.0) {

            const int sampleCount = uniforms.sampleCount;

            for (int i = 0; i < sampleCount; i++) {
                int sampleIdx = int(uniforms.frameSeed / 4) * sampleCount + i;
                vec2 blueNoiseVec = vec2(
                    SampleBlueNoise(highResPixel, sampleIdx, 0, scramblingRankingTexture, sobolSequenceTexture),
                    SampleBlueNoise(highResPixel, sampleIdx, 1, scramblingRankingTexture, sobolSequenceTexture)
                    );

                float alpha = sqr(max(0.0, material.roughness));

                vec3 V = normalize(-viewVec);
                vec3 N = worldNorm;

                Surface surface = CreateSurface(V, N, vec3(1.0), material);

                Ray ray;
                ray.ID = i;
                blueNoiseVec.y *= (1.0 - uniforms.bias);

                float pdf = 1.0;
                BRDFSample brdfSample;
                if (material.roughness >= 0.0) {
                    ImportanceSampleGGXVNDF(blueNoiseVec, N, V, alpha,
                        ray.direction, pdf);
                }
                else {
                    ray.direction = normalize(reflect(-V, N));
                }

                bool isRayValid = !isnan(ray.direction.x) || !isnan(ray.direction.y) || 
                    !isnan(ray.direction.z) || dot(N, ray.direction) >= 0.0;

                if (isRayValid) {
                    // Scale offset by depth since the depth buffer inaccuracies increase at a distance and might not match the ray traced geometry anymore
                    float viewOffset = max(1.0, length(viewPos));
                    ray.origin = worldPos + ray.direction * EPSILON * viewOffset + worldNorm * EPSILON * viewOffset;

                    ray.hitID = -1;
                    ray.hitDistance = 0.0;

                    vec3 radiance = vec3(0.0);

                    if (material.roughness <= uniforms.roughnessCutoff) {
#ifdef OPACITY_CHECK
                        HitClosestTransparency(ray, INSTANCE_MASK_ALL, 0.0, INF);
#else
                        HitClosest(ray, INSTANCE_MASK_ALL, 0.0, INF);
#endif
                        hitDistance = ray.hitDistance;
                        radiance = EvaluateHit(ray);
                    }
                    else {
#ifdef DDGI
                        radiance = GetLocalIrradiance(worldPos, V, N).rgb;
                        radiance = IsInsideVolume(worldPos) ? radiance : vec3(0.0);
#endif
                    }

#ifdef AUTO_EXPOSURE
                    float radianceLimit = 9.6 * uniforms.radianceLimit * texelFetch(exposureTexture, ivec2(0), 0).r;
#else
                    float radianceLimit = uniforms.radianceLimit;
#endif

                    float radianceMax = max(max(max(radiance.r, 
                        max(radiance.g, radiance.b)), radianceLimit), 1e-12);
                    reflection.rgb += radiance * (radianceLimit / radianceMax);
                }
            }

            reflection.rgb /= float(sampleCount);

        }

        imageStore(rtrImage, pixel, vec4(reflection.rgb, hitDistance));
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
    Instance instance = GetInstance(ray);
    Triangle tri = GetTriangle(ray, instance);

    bool backfaceHit;
    Surface surface = GetSurfaceParameters(instance, tri, ray, false, backfaceHit, uniforms.textureLevel);

    // Evaluate indirect lighting
#ifdef DDGI
    // Trick: Offset on secondary bounce towards camera, avoids light leaking (introducing innacuracies ofc)
    vec3 V = normalize(globalData.cameraLocation.xyz - surface.P);
    vec3 irradiance = GetLocalIrradiance(surface.P, V, surface.N, surface.geometryNormal).rgb;
    // Approximate indirect specular for ray by using the irradiance grid
    // This enables metallic materials to have some kind of secondary reflection
    float ddgiDistanceDamp = min(1.0, sqr(distance(surface.P, ray.origin)));
    surface.NdotV = saturate(dot(surface.N, surface.V));
    vec3 indirect = EvaluateIndirectDiffuseBRDF(surface) * irradiance +
        EvaluateIndirectSpecularBRDF(surface) * irradiance;
    radiance += IsInsideVolume(surface.P) ? indirect * ddgiDistanceDamp * ddgiData.volumeStrength: vec3(0.0);
#endif
    
    radiance += surface.material.emissiveColor;

    float curSeed = float(uniforms.frameSeed) / 255.0 + float(ray.ID) * float(uniforms.sampleCount);
    // Evaluate direct light
    for (int i = 0; i < uniforms.lightSampleCount; i++) {
        radiance += (EvaluateDirectLight(surface, curSeed) / float(uniforms.lightSampleCount));
        curSeed += 1.0 / float(uniforms.lightSampleCount);
    }

    return radiance;

}

vec3 EvaluateDirectLight(inout Surface surface, inout float seed) {

    if (GetLightCount() == 0)
        return vec3(0.0);
    
    float raySeed = float(gl_GlobalInvocationID.x * uniforms.resolution.y + gl_GlobalInvocationID.y);

    float lightPdf;
    Light light = GetLight(surface, raySeed, seed, lightPdf);

    float solidAngle, lightDistance;
    SampleLight(light, surface, raySeed, seed, solidAngle, lightDistance);

    // Evaluate the BRDF
    vec3 reflectance = EvaluateDiffuseBRDF(surface) + EvaluateSpecularBRDF(surface);
    reflectance *= surface.material.opacity;
    vec3 radiance = light.radiance * solidAngle;

    // Check for visibilty. This is important to get an
    // estimate of the solid angle of the light from point P
    // on the surface.
#ifdef USE_SHADOW_MAP
    radiance *= CalculateShadowWorldSpace(uniforms.shadow, cascadeMaps, surface.P,
        surface.geometryNormal, saturate(dot(surface.L, surface.geometryNormal)));
#else
    if (light.castShadow)
        radiance *= CheckVisibility(surface, lightDistance);
#endif

#ifdef CLOUD_SHADOWS
    if (light.type == uint(DIRECTIONAL_LIGHT)) {
        vec3 P = vec3(globalData.vMatrix * vec4(surface.P, 1.0));
        float cloudShadowFactor = CalculateCloudShadow(P, cloudShadowUniforms.cloudShadow, cloudMap);

        radiance *= cloudShadowFactor;
    }
#endif
    
    return reflectance * radiance * surface.NdotL / lightPdf;

}

float CheckVisibility(Surface surface, float lightDistance) {

    if (surface.NdotL > 0.0) {
        Ray ray;
        ray.direction = surface.L;
        ray.origin = surface.P + surface.N * EPSILON;
        return HitAnyTransparency(ray, INSTANCE_MASK_SHADOW, 0.0, lightDistance - 2.0 * EPSILON);
    }
    else {
        return 0.0;
    }

}