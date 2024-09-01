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

layout (local_size_x = 8, local_size_y = 4) in;

layout (set = 3, binding = 0, rgba16f) writeonly uniform image2D rtrImage;

layout(set = 3, binding = 1) uniform sampler2D normalTexture;
layout(set = 3, binding = 2) uniform sampler2D depthTexture;
layout(set = 3, binding = 3) uniform sampler2D roughnessMetallicAoTexture;
layout(set = 3, binding = 4) uniform isampler2D offsetTexture;
layout(set = 3, binding = 5) uniform usampler2D materialIdxTexture;
layout(set = 3, binding = 6) uniform sampler2DArrayShadow cascadeMaps;

layout(set = 3, binding = 7) uniform sampler2D scramblingRankingTexture;
layout(set = 3, binding = 8) uniform sampler2D sobolSequenceTexture;

const ivec2 offsets[4] = ivec2[4](
    ivec2(0, 0),
    ivec2(1, 0),
    ivec2(0, 1),
    ivec2(1, 1)
);

layout(std140, set = 3, binding = 9) uniform UniformBuffer {
    float radianceLimit;
    uint frameSeed;
    float bias;
    int lightSampleCount;
    int textureLevel;
    float roughnessCutoff;
    int halfRes;
    int padding0;
    int padding1;
    int padding2;
    ivec2 resolution;
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
        
        vec2 texCoord = (vec2(pixel) + vec2(0.5)) / vec2(resolution);

        // No need, there is no offset right now
        int offsetIdx = texelFetch(offsetTexture, pixel, 0).r;
        ivec2 offset = offsets[offsetIdx];

        float depth = texelFetch(depthTexture, pixel, 0).r;

        vec2 recontructTexCoord;
        if (uniforms.halfRes > 0)
            recontructTexCoord = (2.0 * (vec2(pixel)) + offset + 0.5) / (2.0 * vec2(resolution));
        else
            recontructTexCoord = (vec2(pixel) + 0.5) / (vec2(resolution));
            
        vec3 viewPos = ConvertDepthToViewSpace(depth, recontructTexCoord);
        vec3 worldPos = vec3(globalData.ivMatrix * vec4(viewPos, 1.0));
        vec3 viewVec = vec3(globalData.ivMatrix * vec4(viewPos, 0.0));
        vec3 worldNorm = normalize(vec3(globalData.ivMatrix * vec4(DecodeNormal(textureLod(normalTexture, texCoord, 0).rg), 0.0)));

        int sampleIdx = int(uniforms.frameSeed);
        vec2 blueNoiseVec = vec2(
            SampleBlueNoise(pixel, sampleIdx, 0, scramblingRankingTexture, sobolSequenceTexture),
            SampleBlueNoise(pixel, sampleIdx, 1, scramblingRankingTexture, sobolSequenceTexture)
            );

        uint materialIdx = texelFetch(materialIdxTexture, pixel, 0).r;
        Material material = UnpackMaterial(materialIdx);

        float roughness = texelFetch(roughnessMetallicAoTexture, pixel, 0).r;
        material.roughness *= material.roughnessMap ? roughness : 1.0;

        vec3 reflection = vec3(0.0);

        if (material.roughness <= 1.0 && depth < 1.0) {

            float alpha = sqr(material.roughness);

            vec3 V = normalize(-viewVec);
            vec3 N = worldNorm;

            Surface surface = CreateSurface(V, N, vec3(1.0), material);

            Ray ray;
            blueNoiseVec.y *= (1.0 - uniforms.bias);

            float pdf = 1.0;
            BRDFSample brdfSample;
            if (material.roughness > 0.01) {
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
                float viewOffset = max(1.0, length(viewPos)) * 0.1;
                ray.origin = worldPos + ray.direction * EPSILON * 0.1 * viewOffset + worldNorm * EPSILON * viewOffset * 0.1;

                ray.hitID = -1;
                ray.hitDistance = 0.0;

                vec3 radiance = vec3(0.0);

                if (material.roughness <= uniforms.roughnessCutoff) {
    #ifdef OPACITY_CHECK
                    HitClosestTransparency(ray, INSTANCE_MASK_ALL, 0.0, INF);
    #else
                    HitClosest(ray, INSTANCE_MASK_ALL, 0.0, INF);
    #endif

                    radiance = EvaluateHit(ray);
                }
                else {
    #ifdef DDGI
                    radiance = GetLocalIrradiance(worldPos, V, N).rgb;
                    radiance = IsInsideVolume(worldPos) ? radiance : vec3(0.0);
    #endif
                }

                float radianceMax = max(max(max(radiance.r, 
                    max(radiance.g, radiance.b)), uniforms.radianceLimit), 0.01);
                reflection = radiance * (uniforms.radianceLimit / radianceMax);
            }

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
    Instance instance = GetInstance(ray);
    Triangle tri = GetTriangle(ray, instance);

    bool backfaceHit;
    Surface surface = GetSurfaceParameters(instance, tri, ray, false, backfaceHit, uniforms.textureLevel);
    
    radiance += surface.material.emissiveColor;

    float curSeed = float(uniforms.frameSeed) / 255.0;
    // Evaluate direct light
    for (int i = 0; i < uniforms.lightSampleCount; i++) {
        radiance += EvaluateDirectLight(surface, curSeed);
    }

    radiance /= float(uniforms.lightSampleCount);

    // Evaluate indirect lighting
#ifdef DDGI
    vec3 irradiance = GetLocalIrradiance(surface.P, surface.V, surface.N).rgb;
    // Approximate indirect specular for ray by using the irradiance grid
    // This enables metallic materials to have some kind of secondary reflection
    vec3 indirect = EvaluateIndirectDiffuseBRDF(surface) * irradiance +
        EvaluateIndirectSpecularBRDF(surface) * irradiance;
    radiance += IsInsideVolume(surface.P) ? indirect * ddgiData.volumeStrength : vec3(0.0);
#endif

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
    radiance *= CheckVisibility(surface, lightDistance);
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