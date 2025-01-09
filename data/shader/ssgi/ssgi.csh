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
#include <../common/traceScreenSpace.hsh>

#include <../brdf/brdfEval.hsh>
#include <../brdf/brdfSample.hsh>
#include <../brdf/importanceSample.hsh>
#include <../brdf/surface.hsh>

#include <../ddgi/ddgi.hsh>
#include <../shadow.hsh>

layout (local_size_x = 8, local_size_y = 4) in;

layout(set = 3, binding = 0, rgba16f) writeonly uniform image2D giImage;

layout(set = 3, binding = 1) uniform sampler2D normalTexture;
layout(set = 3, binding = 2) uniform sampler2D depthTexture;
layout(set = 3, binding = 3) uniform sampler2D roughnessMetallicAoTexture;
layout(set = 3, binding = 4) uniform isampler2D offsetTexture;
layout(set = 3, binding = 5) uniform usampler2D materialIdxTexture;
layout(set = 3, binding = 6) uniform sampler2D directLightTexture;

layout(set = 3, binding = 7) uniform sampler2D scramblingRankingTexture;
layout(set = 3, binding = 8) uniform sampler2D sobolSequenceTexture;

layout(set = 1, binding = 12) uniform samplerCube diffuseProbe;

#ifdef AUTO_EXPOSURE
layout(set = 3, binding = 13) uniform sampler2D exposureTexture;
#endif

const ivec2 offsets[4] = ivec2[4](
ivec2(0, 0),
ivec2(1, 0),
ivec2(0, 1),
ivec2(1, 1)
);

layout(std140, set = 3, binding = 9) uniform UniformBuffer {
    float radianceLimit;
    uint frameSeed;
    float radius;
    uint rayCount;
    uint sampleCount;
    int downsampled2x;
} uniforms;

float Luma(vec3 color) {

    const vec3 luma = vec3(0.299, 0.587, 0.114);
    return dot(color, luma);

}

void main() {

    ivec2 resolution = ivec2(imageSize(giImage));

    if (int(gl_GlobalInvocationID.x) < resolution.x &&
        int(gl_GlobalInvocationID.y) < resolution.y) {

        ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

        vec2 texCoord = (vec2(pixel) + vec2(0.5)) / vec2(resolution);

        int offsetIdx = texelFetch(offsetTexture, pixel, 0).r;
        ivec2 pixelOffset = offsets[offsetIdx];

        float depth = texelFetch(depthTexture, pixel, 0).r;

        vec2 recontructTexCoord = (2.0 * vec2(pixel) + pixelOffset + vec2(0.5)) / (2.0 * vec2(resolution));
        vec3 viewPos = ConvertDepthToViewSpace(depth, texCoord);
        vec3 worldPos = vec3(globalData.ivMatrix * vec4(viewPos, 1.0));
        vec3 viewVec = vec3(globalData.ivMatrix * vec4(viewPos, 0.0));
        vec3 viewNorm = normalize(DecodeNormal(textureLod(normalTexture, texCoord, 0).rg));
        vec3 worldNorm = normalize(vec3(globalData.ivMatrix * vec4(viewNorm, 0.0)));
        vec3 worldView = -normalize(vec3(globalData.ivMatrix * vec4(viewPos, 0.0)));

        uint materialIdx = texelFetch(materialIdxTexture, pixel, 0).r;
        Material material = UnpackMaterial(materialIdx);
        
        vec3 globalProbeFallback = textureLod(diffuseProbe, worldNorm, 0).rgb;
#ifdef DDGI
        //rayIrradiance = GetLocalIrradianceInterpolated(worldPos, -V, N, N, globalProbeFallback).rgb * ddgiData.volumeStrength;
        vec3 probeIrradiance = GetLocalIrradianceInterpolated(worldPos, worldView, worldNorm,
             worldNorm, globalProbeFallback).rgb * ddgiData.volumeStrength;
        probeIrradiance = IsInsideVolume(worldPos) ? probeIrradiance : globalProbeFallback;
#else
        vec3 probeIrradiance = globalProbeFallback;
#endif

        vec3 irradiance = vec3(0.0);
        float hits = 0.0;
        float aoHits = 0.0;

        if (depth < 1.0) {

#ifdef AUTO_EXPOSURE
            float radianceLimit = 9.6 * uniforms.radianceLimit * texelFetch(exposureTexture, ivec2(0), 0).r;
#else
            float radianceLimit = uniforms.radianceLimit;
#endif

            vec3 V = normalize(-viewVec);
            vec3 N = worldNorm;

            Surface surface = CreateSurface(V, N, vec3(1.0), material);

            for (uint j = 0; j < uniforms.rayCount; j++) {
                // Delivers better results than frameCount * rayCount
                int sampleIdx = int(globalData.frameCount + j);
                vec3 blueNoiseVec = vec3(
                    SampleBlueNoise(pixel, sampleIdx, 0, scramblingRankingTexture, sobolSequenceTexture),
                    SampleBlueNoise(pixel, sampleIdx, 1, scramblingRankingTexture, sobolSequenceTexture),
                    SampleBlueNoise(pixel, sampleIdx, 2, scramblingRankingTexture, sobolSequenceTexture)
                );

                Ray ray;

                float pdf = 1.0;
                float NdotL;
                ImportanceSampleCosDir(N, blueNoiseVec.xy, 
                    ray.direction, NdotL, pdf);

                ray.hitID = -1;
                ray.hitDistance = 0.0;

                // We could also use ray tracing here
                float rayLength = uniforms.radius;
                
                vec3 viewDir = normalize(vec3(globalData.vMatrix * vec4(ray.direction, 0.0)));
                float viewOffset = max(1.0, length(viewPos));
                vec3 viewRayOrigin = viewPos + 5.0 * viewNorm * EPSILON * viewOffset + viewDir * EPSILON * viewOffset;
                
                vec2 hitPixel;
                vec3 hitPoint;
                float jitter =  GetInterleavedGradientNoise(vec2(pixel)) / float(uniforms.rayCount) + j / float(uniforms.rayCount);
                if (traceScreenSpaceAdvanced(viewRayOrigin, viewDir, depthTexture, 4.0, 16.0, jitter, 64.0, 1000.0, false, false, hitPixel, hitPoint)) {
                    vec2 hitTexCoord =  vec2(hitPixel + 0.5) / vec2(textureSize(depthTexture, 0));
                    vec3 stepViewNorm = normalize(DecodeNormal(texelFetch(normalTexture, ivec2(hitPixel), 0).rg));
                    float depth = texelFetch(depthTexture, ivec2(hitPixel), 0).r;
                    hitPoint = ConvertDepthToViewSpace(depth, hitTexCoord);
                        
                    float NdotV = saturate(dot(-viewDir, stepViewNorm));
                    NdotL = saturate(dot(viewNorm, hitPoint));
                    if (NdotV > 0.0) {
                        irradiance += textureLod(directLightTexture, hitTexCoord, 0).rgb;
                    }
                    else {
                        aoHits += 1.0;
                    }
                    hits += 1.0;
                }
            }

            float irradianceMax = max(max(max(irradiance.r,
                max(irradiance.g, irradiance.b)), radianceLimit), 1e-12);
            irradiance *= (radianceLimit / irradianceMax);
            
            // Supplement misses with the probe irradiance
            irradiance += float(uniforms.rayCount - hits) * probeIrradiance;
            irradiance /= (float(uniforms.rayCount));
            
        }

        float ao = max(1.0 - (aoHits / float(uniforms.rayCount)), 0.0);

        imageStore(giImage, pixel, vec4(vec3(irradiance), ao));
    }

}