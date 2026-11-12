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
#include <../brdf/transmission.hsh>
#include <../brdf/brdfSample.hsh>
#include <../brdf/importanceSample.hsh>
#include <../brdf/surface.hsh>

#include <../ddgi/ddgi.hsh>
#include <../ddgi/transmission.hsh>
#include <../clouds/shadow.hsh>

#ifndef RAY_TRACING_PIPELINE
layout (local_size_x = 8, local_size_y = 4) in;
#endif

layout (set = 3, binding = 0, rgba16f) uniform image2D rtrImage;

layout(set = 3, binding = 1) uniform sampler2D normalTexture;
layout(set = 3, binding = 2) uniform sampler2D depthTexture;
layout(set = 3, binding = 3) uniform sampler2D roughnessMetallicAoTexture;
layout(set = 3, binding = 4) uniform isampler2D offsetTexture;
layout(set = 3, binding = 5) uniform usampler2D materialIdxTexture;

layout(set = 3, binding = 7) uniform sampler2D scramblingRankingTexture;
layout(set = 3, binding = 8) uniform sampler2D sobolSequenceTexture;

#ifdef CLOUD_SHADOWS
layout(set = 3, binding = 9) uniform sampler2D cloudMap;
#endif

#ifdef AUTO_EXPOSURE
layout(set = 3, binding = 11) uniform sampler2D exposureTexture;
#endif

layout(set = 1, binding = 11) uniform samplerCube specularProbe;

const ivec2 offsets[9] = ivec2[9](
    ivec2(-1, -1),
    ivec2(0, -1),
    ivec2(1, -1),
    ivec2(-1, 0),
    ivec2(0, 0),
    ivec2(1, 0),
    ivec2(-1, 1),
    ivec2(0, 1),
    ivec2(1, 1)
);

const ivec2 pixelOffsets[4] = ivec2[4](
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
    uint frameCount;
    int padding1;
} uniforms;

#include <../raytracer/lighting.hsh>

vec3 EvaluateHit(inout Ray ray, float rand);

void main() {

    ivec2 resolution = uniforms.resolution;

    if (int(INVOCATION_ID.x) < resolution.x &&
        int(INVOCATION_ID.y) < resolution.y) {

        ivec2 pixel = ivec2(INVOCATION_ID.xy);

#ifdef SSR
        vec4 reflection = imageLoad(rtrImage, pixel);
#else
        vec4 reflection = vec4(0.0);
#endif

        int offsetIdx = texelFetch(offsetTexture, pixel, 0).r;
#ifdef UPSCALE
        ivec2 offset = pixelOffsets[globalData.frameCount % 4];
#else
        ivec2 offset = ivec2(0);
#endif

        ivec2 highResPixel;
        vec2 recontructTexCoord;
        if (uniforms.halfRes > 0) {            
#ifdef UPSCALE
            recontructTexCoord = (2.0 * (vec2(pixel)) + offset + 0.5) / (2.0 * vec2(resolution));
            highResPixel = 2 * pixel + offset;
#else
            // The downsampled data belongs to one pixel of the 2x2 block
            recontructTexCoord = (2.0 * vec2(pixel) + pixelOffsets[offsetIdx] + 0.5) / (2.0 * vec2(resolution));
            highResPixel = pixel;
#endif
        }
        else {
            recontructTexCoord = (vec2(pixel) + 0.5) / vec2(resolution);
            highResPixel = pixel;
        }

        float depth = texelFetch(depthTexture, highResPixel, 0).r;
            
        vec3 viewPos = ConvertDepthToViewSpace(depth, recontructTexCoord);
        vec3 worldPos = vec3(globalData.ivMatrix * vec4(viewPos, 1.0));
        vec3 viewVec = vec3(globalData.ivMatrix * vec4(viewPos, 0.0));
        vec3 worldNorm = normalize(vec3(globalData.ivMatrix * 
            vec4(DecodeNormal(texelFetch(normalTexture, highResPixel, 0).rg), 0.0)));

        uint materialIdx = texelFetch(materialIdxTexture, highResPixel, 0).r;
        Material material = UnpackMaterial(materialIdx);

        float roughness = texelFetch(roughnessMetallicAoTexture, highResPixel, 0).r;
        material.roughness *= material.roughnessMap || material.terrain ? roughness : 1.0;

        float hitDistance = reflection.a;
#ifdef DEFERRED_SHADOW_RAYS
        int pixelIdx = pixel.y * resolution.x + pixel.x;
#endif

        if (material.roughness <= 1.0 && depth < 1.0 && reflection.a == 0.0) {

            vec3 V = normalize(-viewVec);
            vec3 N = worldNorm;

            vec3 R = normalize(reflect(-V, N));
            float mipLevel = material.roughness * float(4 - 1);
            vec3 prefilteredSpecular = textureLod(specularProbe, R, 0.0).rgb;

            const int sampleCount = uniforms.sampleCount;

            // Values per pixel are computed once before the loop
            float roughness = material.roughness;
            float alpha = sqr(max(0.0, roughness));
            float viewOffset = max(1.0, 2.0 * length(viewPos));
#ifdef AUTO_EXPOSURE
            float radianceLimit = 9.6 * uniforms.radianceLimit * texelFetch(exposureTexture, ivec2(0), 0).r;
#else
            float radianceLimit = uniforms.radianceLimit;
#endif

            for (int i = 0; i < sampleCount; i++) {
#ifdef UPSCALE
                int sampleIdx = int(uniforms.frameSeed / 4) * sampleCount + i;
#else
                int sampleIdx = int(uniforms.frameSeed) * sampleCount + i;
#endif
#ifdef DEFERRED_SHADOW_RAYS
                int deferredSampleIdx = pixelIdx * sampleCount + i;
                deferredSamples[deferredSampleIdx].radiance.w = 0.0;
                deferredLightBase = deferredSampleIdx * uniforms.lightSampleCount;
                for (int j = 0; j < uniforms.lightSampleCount; j++)
                    deferredLights[deferredLightBase + j] = vec4(0.0, 0.0, 0.0, 1.0);
#endif
                vec3 blueNoiseVec = vec3(
                    SampleBlueNoise(highResPixel, sampleIdx, 0, scramblingRankingTexture, sobolSequenceTexture),
                    SampleBlueNoise(highResPixel, sampleIdx, 1, scramblingRankingTexture, sobolSequenceTexture),
                    SampleBlueNoise(highResPixel, sampleIdx, 2, scramblingRankingTexture, sobolSequenceTexture)
                    );

                Ray ray;
                ray.ID = i;
                blueNoiseVec.y *= (1.0 - uniforms.bias);

                float pdf = 1.0;
                BRDFSample brdfSample;
                if (roughness >= 0.0) {
                    ImportanceSampleGGXVNDF(blueNoiseVec.xy, N, V, alpha,
                        ray.direction, pdf);
                }
                else {
                    ray.direction = R;
                }

                bool isRayValid = !isnan3(ray.direction) && !isinf3(ray.direction) &&
                    dot(N, ray.direction) >= 0.0;

                if (isRayValid) {
                    // Scale offset by depth since the depth buffer inaccuracies increase at a distance and might not match the ray traced geometry anymore
                    ray.origin = worldPos + ray.direction * EPSILON * viewOffset + worldNorm * EPSILON * viewOffset;

                    ray.hitID = -1;
                    ray.hitDistance = 0.0;

                    vec3 radiance = vec3(0.0);

                    if (roughness <= uniforms.roughnessCutoff) {
                        HitClosestOpacityCheck(ray, INSTANCE_MASK_ALL, 0.0, INF);
                        // Misses have an infinite distance, which breaks the virtual reprojection of the temporal pass
                        hitDistance = ray.hitID != -1 ? ray.hitDistance : 0.0;
                        radiance = EvaluateHit(ray, blueNoiseVec.z);
                    }
                    else {
                        vec4 probeIrradiance, probeRadiance;
                        GetLocalProbeLighting(worldPos, V, N, ray.direction, N, blueNoiseVec.z, 
                            prefilteredSpecular, prefilteredSpecular, probeIrradiance, probeRadiance);
#if defined(RADIANCE_VOLUME)
                        radiance = probeRadiance.rgb;
#elif defined(IRRADIANCE_VOLUME)
                        radiance = IsInsideVolume(worldPos) ? probeIrradiance.rgb : vec3(0.0);
#endif
                        hitDistance = 1.0;
                    }

#ifdef DEFERRED_SHADOW_RAYS
                    deferredSamples[deferredSampleIdx].radiance = vec4(radiance, 1.0);
                    deferredSamples[deferredSampleIdx].lobeWeight = vec4(1.0, 1.0, 1.0, radianceLimit);
                    continue;
#endif

                    float radianceMax = max(max(max(radiance.r, 
                        max(radiance.g, radiance.b)), radianceLimit), 1e-12);
                    reflection.rgb += radiance * (radianceLimit / radianceMax);
                }
            }

#ifndef DEFERRED_SHADOW_RAYS
            reflection.rgb /= float(sampleCount);
#endif

        }
#ifdef DEFERRED_SHADOW_RAYS
        else {
            deferredSamples[pixelIdx * uniforms.sampleCount].radiance.w = -1.0;
        }
#endif

        imageStore(rtrImage, pixel, vec4(reflection.rgb, hitDistance));
    }

}

vec3 EvaluateHit(inout Ray ray, float rand) {

    vec3 radiance = vec3(0.0);
    
    // If we didn't find a triangle along the ray,
    // we add the contribution of the environment map
    if (ray.hitID == -1) {
        return SampleEnvironmentMap(ray.direction).rgb;
    }
    
    bool backfaceHit;
    Surface surface = GetHitSurface(ray, false, backfaceHit, uniforms.textureLevel);

    // Evaluate indirect lighting
#ifdef IRRADIANCE_VOLUME
    // Trick: Offset on secondary bounce towards camera, avoids light leaking (introducing innacuracies ofc)
    vec3 V = normalize(globalData.cameraLocation.xyz - surface.P);
    vec4 probeIrradiance, probeRadiance;
    GetLocalProbeLighting(surface.P, V, surface.N, surface.N, surface.geometryNormal, rand, 
        vec3(0.0), vec3(0.0), probeIrradiance, probeRadiance);
    // Approximate indirect specular for ray by using the irradiance grid
    // This enables metallic materials to have some kind of secondary reflection
    float ddgiDistanceDamp = min(1.0, sqr(distance(surface.P, ray.origin)));
    surface.NdotV = saturate(dot(surface.N, surface.V));
    vec3 indirect = EvaluateIndirectDiffuseBRDF(surface) * probeIrradiance.rgb +
        EvaluateIndirectSpecularBRDF(surface) * probeRadiance.rgb;
    indirect += GetTransmittedIndirectDiffuse(surface);
    radiance += IsInsideVolume(surface.P) ? indirect * ddgiDistanceDamp * ddgiData.volumeStrength: vec3(0.0);
#endif
    
    radiance += surface.material.emissiveColor;

    float curSeed = float(uniforms.frameSeed) / 255.0 + float(ray.ID) * float(uniforms.sampleCount);
    float raySeed = float(INVOCATION_ID.x * uniforms.resolution.y + INVOCATION_ID.y);
    radiance += EvaluateDirectLights(surface, raySeed, curSeed, uniforms.lightSampleCount);

    return radiance;

}
