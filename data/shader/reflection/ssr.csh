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
#include <../common/traceScreenSpace.hsh>

#include <../brdf/brdfEval.hsh>
#include <../brdf/brdfSample.hsh>
#include <../brdf/importanceSample.hsh>
#include <../brdf/surface.hsh>

#include <../ddgi/ddgi.hsh>
#include <../shadow.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

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
    int padding0;
    int padding1;
    ivec2 resolution;
    Shadow shadow;
} uniforms;

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
        vec3 viewNormal = DecodeNormal(textureLod(normalTexture, texCoord, 0).rg);
        vec3 worldNorm = normalize(vec3(globalData.ivMatrix * vec4(viewNormal, 0.0)));

        uint materialIdx = texelFetch(materialIdxTexture, pixel, 0).r;
        Material material = UnpackMaterial(materialIdx);

        float roughness = texelFetch(roughnessMetallicAoTexture, pixel, 0).r;
        material.roughness *= material.roughnessMap ? roughness : 1.0;

        vec3 reflection = vec3(0.0);
        float hits = 0.0;

        if (material.roughness <= 1.0 && depth < 1.0) {

            const int sampleCount = uniforms.sampleCount;

            for (int i = 0; i < sampleCount; i++) {
                int sampleIdx = int(uniforms.frameSeed) * sampleCount + i;
                vec2 blueNoiseVec = vec2(
                    SampleBlueNoise(pixel, sampleIdx, 0, scramblingRankingTexture, sobolSequenceTexture),
                    SampleBlueNoise(pixel, sampleIdx, 1, scramblingRankingTexture, sobolSequenceTexture)
                    );

                float alpha = sqr(material.roughness);

                vec3 V = normalize(-viewVec);
                vec3 N = worldNorm;

                Surface surface = CreateSurface(V, N, vec3(1.0), material);

                Ray ray;
                ray.ID = i;
                blueNoiseVec.y *= (1.0 - uniforms.bias);

                float pdf = 1.0;
                BRDFSample brdfSample;
                if (material.roughness > 0.01) {
                    ImportanceSampleGGXVNDF(blueNoiseVec.xy, N, V, alpha,
                        ray.direction, pdf);
                }
                else {
                    ray.direction = normalize(reflect(-V, N));
                }

                vec3 viewDir = normalize(vec3(globalData.vMatrix * vec4(ray.direction, 0.0)));

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
                        vec3 viewRayOrigin = viewPos + 10.0 * viewNormal * EPSILON * viewOffset + viewDir * EPSILON * viewOffset;

                        vec2 hitPixel;
                        vec3 hitPoint;
                        float jitter =  GetInterleavedGradientNoise(vec2(pixel), 4u) / float(sampleCount) + i / float(sampleCount);
                        if (traceScreenSpaceAdvanced(viewRayOrigin, viewDir, depthTexture, 1.0, 16.0, jitter, 32.0, 2000.0, hitPixel, hitPoint)) {
                            vec2 hitTexCoord =  vec2(hitPixel + 0.5) / vec2(textureSize(depthTexture, 0));
                            radiance = textureLod(lightingTexture, hitTexCoord, 1).rgb;
                            hits += 1.0;
                        }
                    }

                    float radianceMax = max(max(max(radiance.r, 
                        max(radiance.g, radiance.b)), uniforms.radianceLimit), 0.01);
                    reflection += radiance * (uniforms.radianceLimit / radianceMax);
                }
            }

            reflection /= float(sampleCount);

        }

        imageStore(rtrImage, pixel, vec4(reflection, hits));
    }

}