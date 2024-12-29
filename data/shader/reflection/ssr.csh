#define EPSILON 0.001

#include <../globals.hsh>

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

layout (set = 3, binding = 0, rgba16f) writeonly uniform image2D rtrImage;

layout(set = 3, binding = 1) uniform sampler2D normalTexture;
layout(set = 3, binding = 2) uniform sampler2D depthTexture;
layout(set = 3, binding = 3) uniform sampler2D roughnessMetallicAoTexture;
layout(set = 3, binding = 4) uniform isampler2D offsetTexture;
layout(set = 3, binding = 5) uniform usampler2D materialIdxTexture;
layout(set = 3, binding = 6) uniform sampler2DArrayShadow cascadeMaps;

layout(set = 3, binding = 7) uniform sampler2D scramblingRankingTexture;
layout(set = 3, binding = 8) uniform sampler2D sobolSequenceTexture;

layout(set = 3, binding = 9) uniform sampler2D lightingTexture;
#ifdef AUTO_EXPOSURE
layout(set = 3, binding = 11) uniform sampler2D exposureTexture;
#endif

// From deferred.hsh
layout(set = 1, binding = 11) uniform samplerCube specularProbe;

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
    uint frameCount;
    int padding1;
    Shadow shadow;
} uniforms;

vec4 SampleCatmullRom(vec2 uv) {

    // http://advances.realtimerendering.com/s2016/Filmic%20SMAA%20v7.pptx
    // Credit: Jorge Jimenez (SIGGRAPH 2016)
    // Ignores the 4 corners of the 4x4 grid
    // Learn more: http://vec3.ca/bicubic-filtering-in-fewer-taps/
    vec2 resolution = vec2(textureSize(lightingTexture, 0));
    vec2 position = uv * resolution;

    vec2 center = floor(position - 0.5) + 0.5;
    vec2 f = position - center;
    vec2 f2 = f * f;
    vec2 f3 = f2 * f;

    vec2 w0 = f2 - 0.5 * (f3 + f);
    vec2 w1 = 1.5 * f3 - 2.5 * f2 + 1.0;
    vec2 w3 = 0.5 * (f3 - f2);
    vec2 w2 = 1.0 - w0 - w1 - w3;

    vec2 w12 = w1 + w2;

    vec2 tc0 = (center - 1.0) / resolution;
    vec2 tc12 = (center + w2 / w12) / resolution;
    vec2 tc3 = (center + 2.0) / resolution;

    vec2 uv0 = clamp(vec2(tc12.x, tc0.y), vec2(0.0), vec2(1.0));
    vec2 uv1 = clamp(vec2(tc0.x, tc12.y), vec2(0.0), vec2(1.0));
    vec2 uv2 = clamp(vec2(tc12.x, tc12.y), vec2(0.0), vec2(1.0));
    vec2 uv3 = clamp(vec2(tc3.x, tc12.y), vec2(0.0), vec2(1.0));
    vec2 uv4 = clamp(vec2(tc12.x, tc3.y), vec2(0.0), vec2(1.0));

    float weight0 = w12.x * w0.y;
    float weight1 = w0.x * w12.y;
    float weight2 = w12.x * w12.y;
    float weight3 = w3.x * w12.y;
    float weight4 = w12.x * w3.y;

    vec4 sample0 = texture(lightingTexture, uv0) * weight0;
    vec4 sample1 = texture(lightingTexture, uv1) * weight1;
    vec4 sample2 = texture(lightingTexture, uv2) * weight2;
    vec4 sample3 = texture(lightingTexture, uv3) * weight3;
    vec4 sample4 = texture(lightingTexture, uv4) * weight4;

    float totalWeight = weight0 + weight1 + 
        weight2 + weight3 + weight4;

    vec4 totalSample = sample0 + sample1 +
        sample2 + sample3 + sample4;

    return totalSample / totalWeight;    

}

shared int maxSteps;
shared int stepSize;

void main() {

    ivec2 resolution = uniforms.resolution;

    if (gl_LocalInvocationIndex == 0u) {
        maxSteps = 0;
        stepSize = 0;
    }

    barrier();

    if (int(gl_GlobalInvocationID.x) < resolution.x &&
        int(gl_GlobalInvocationID.y) < resolution.y) {

        ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

        // No need, there is no offset right now
        int offsetIdx = texelFetch(offsetTexture, pixel, 0).r;
#ifdef UPSCALE
        ivec2 offset = offsets[globalData.frameCount % 4];
#else
        ivec2 offset = ivec2(0);
#endif

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

        float depth = texelFetch(depthTexture, highResPixel, 0).r;
            
        vec3 viewPos = ConvertDepthToViewSpace(depth, recontructTexCoord);
        vec3 worldPos = vec3(globalData.ivMatrix * vec4(viewPos, 1.0));
        vec3 viewVec = vec3(globalData.ivMatrix * vec4(viewPos, 0.0));
        vec3 viewNormal = normalize(DecodeNormal(textureLod(normalTexture, recontructTexCoord, 0).rg));
        vec3 worldNorm = normalize(vec3(globalData.ivMatrix * vec4(viewNormal, 0.0)));

        uint materialIdx = texelFetch(materialIdxTexture, highResPixel, 0).r;
        Material material = UnpackMaterial(materialIdx);

        float roughness = texelFetch(roughnessMetallicAoTexture, highResPixel, 0).r;
        material.roughness *= material.roughnessMap || material.terrain ? roughness : 1.0;

        if (material.roughness < 0.1) {
            atomicMax(maxSteps, 128);
            atomicMax(stepSize, 8);
        }
        else {
            atomicMax(maxSteps, 64);
            atomicMax(stepSize, 16);
        }

        barrier();

        vec3 reflection = vec3(0.0);
        float hitDistance = 0.0;

        if (material.roughness <= uniforms.roughnessCutoff && depth < 1.0) {

            const int sampleCount = uniforms.sampleCount;

            for (int i = 0; i < sampleCount; i++) {
#ifdef UPSCALE
                int sampleIdx = int(uniforms.frameSeed / 4) * sampleCount + i;
#else
                int sampleIdx = int(uniforms.frameSeed) * sampleCount + i;
#endif
                vec3 blueNoiseVec = vec3(
                    SampleBlueNoise(pixel, sampleIdx, 0, scramblingRankingTexture, sobolSequenceTexture),
                    SampleBlueNoise(pixel, sampleIdx, 1, scramblingRankingTexture, sobolSequenceTexture),
                    SampleBlueNoise(pixel, sampleIdx % 4, 2, scramblingRankingTexture, sobolSequenceTexture)
                    );

                float alpha = sqr(max(0.0, material.roughness));

                vec3 V = normalize(-viewVec);
                vec3 N = worldNorm;

                Surface surface = CreateSurface(V, N, vec3(1.0), material);

                vec3 rayDirection;
                float pdf = 1.0;
                BRDFSample brdfSample;
                if (material.roughness >= 0.05) {
                    ImportanceSampleGGXVNDF(blueNoiseVec.xy, N, V, alpha,
                        rayDirection, pdf);
                }
                else {
                    rayDirection = normalize(reflect(-V, N));
                }

                vec3 viewDir = normalize(vec3(globalData.vMatrix * vec4(rayDirection, 0.0)));

                bool isRayValid = !isnan(rayDirection.x) || !isnan(rayDirection.y) || 
                    !isnan(rayDirection.z) || dot(N, rayDirection) >= 0.0;

                vec3 radiance = vec3(0.0);

                if (isRayValid) {
                    // Scale offset by depth since the depth buffer inaccuracies increase at a distance and might not match the ray traced geometry anymore
                    float viewOffset = max(1.0,2.0 * length(viewPos));
          
                    vec3 viewRayOrigin = viewPos + 2.0 * viewNormal * EPSILON * viewOffset + viewDir * EPSILON * viewOffset;
                    float rayLength = globalData.cameraFarPlane;

                    vec2 hitPixel;
                    vec3 hitPoint;
#ifdef UPSCALE
                    float jitter = GetInterleavedGradientNoise(vec2(highResPixel), 32u) / float(sampleCount) + i / float(sampleCount);
#else
                    float jitter = GetInterleavedGradientNoise(vec2(highResPixel), 32u) / float(sampleCount) + i / float(sampleCount);
#endif
#ifdef RT
                    bool stopBehindGeometry = true;
#else
                    bool stopBehindGeometry = false;
#endif
                    if (traceScreenSpaceAdvanced(viewRayOrigin, viewDir, depthTexture, 1.0, stepSize, 0.5, maxSteps, 
                        rayLength, false, stopBehindGeometry, hitPixel, hitPoint)) {
                        vec2 hitTexCoord = vec2(hitPixel + 0.5) / vec2(textureSize(lightingTexture, 0));

                        //radiance = SampleCatmullRom(hitTexCoord).rgb;
                        radiance = textureLod(lightingTexture, hitTexCoord, 0.0).rgb;
                        hitDistance += distance(hitPoint, viewRayOrigin);
                    }
                    else {
#ifndef RT
                        radiance =  textureLod(specularProbe, rayDirection, 0).rgb;
#endif
                    }
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

            reflection /= float(sampleCount);
            hitDistance /= float(sampleCount);

        }

        imageStore(rtrImage, pixel, vec4(reflection, hitDistance));
    }

}