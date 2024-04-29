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

layout(set = 3, binding = 0, rgba16f) writeonly uniform image2D giImage;

layout(set = 3, binding = 1) uniform sampler2D normalTexture;
layout(set = 3, binding = 2) uniform sampler2D depthTexture;
layout(set = 3, binding = 3) uniform sampler2D roughnessMetallicAoTexture;
layout(set = 3, binding = 4) uniform isampler2D offsetTexture;
layout(set = 3, binding = 5) uniform usampler2D materialIdxTexture;
layout(set = 3, binding = 6) uniform sampler2D directLightTexture;

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
        vec3 worldNorm = normalize(vec3(globalData.ivMatrix * vec4(DecodeNormal(textureLod(normalTexture, texCoord, 0).rg), 0.0)));

        uint materialIdx = texelFetch(materialIdxTexture, pixel, 0).r;
        Material material = UnpackMaterial(materialIdx);

        float roughness = texelFetch(roughnessMetallicAoTexture, pixel, 0).r;
        material.roughness *= material.roughnessMap ? roughness : 1.0;

        vec3 irradiance = vec3(0.0);
        float hits = 0.0;

        if (depth < 1.0) {

            float alpha = sqr(material.roughness);

            vec3 V = normalize(-viewVec);
            vec3 N = worldNorm;

            Surface surface = CreateSurface(V, N, vec3(1.0), material);

            for (uint j = 0; j < uniforms.rayCount; j++) {
                int sampleIdx = int(uniforms.frameSeed *  uniforms.rayCount + j);
                vec3 blueNoiseVec = vec3(
                    SampleBlueNoise(pixel, sampleIdx, 0, scramblingRankingTexture, sobolSequenceTexture),
                    SampleBlueNoise(pixel, sampleIdx, 1, scramblingRankingTexture, sobolSequenceTexture),
                    SampleBlueNoise(pixel, sampleIdx, 2, scramblingRankingTexture, sobolSequenceTexture)
                );

                Ray ray;

                float pdf = 1.0;
                BRDFSample brdfSample = SampleDiffuseBRDF(surface, blueNoiseVec.xy);

                ray.hitID = -1;
                ray.hitDistance = 0.0;

                // We could also use ray tracing here
                float rayLength = uniforms.radius;

                float stepSize = rayLength / float(uniforms.sampleCount);

                ray.direction = brdfSample.L;
                ray.origin = worldPos + surface.N * 0.1 + ray.direction * blueNoiseVec.z * stepSize;

                bool hit = false;
                for (uint i = 0; i < uniforms.sampleCount; i++) {

                    vec3 rayPos = vec3(globalData.vMatrix * vec4(ray.origin + float(i) * ray.direction * stepSize, 1.0));

                    vec4 offset = globalData.pMatrix * vec4(rayPos, 1.0);
                    offset.xyz /= offset.w;
                    vec2 uvPos = offset.xy * 0.5 + 0.5;

                    if (uvPos.x < 0.0 || uvPos.x > 1.0 || uvPos.y < 0.0 || uvPos.y > 1.0)
                        continue;

                    ivec2 stepPixel = ivec2(uvPos * vec2(resolution));
                    float stepDepth = texelFetch(depthTexture, stepPixel, 0).r;

                    vec3 stepPos = ConvertDepthToViewSpace(stepDepth, uvPos);
                    float stepLinearDepth = -stepPos.z;
                    float rayDepth = -rayPos.z;

                    float depthDelta = rayDepth - stepLinearDepth;
                    vec3 worldNorm = normalize(vec3(globalData.ivMatrix * vec4(DecodeNormal(texelFetch(normalTexture, stepPixel, 0).rg), 0.0)));

                    // Check if we are now behind the depth buffer, use that hit as the source of radiance
                    if (stepLinearDepth < rayDepth && abs(depthDelta) < rayLength) {
                        float NdotV = dot(worldNorm, -ray.direction);
                        float NdotL = dot(-worldNorm, surface.N) * 0.5 + 0.5;
                        if (true) {
                            ivec2 lightPixel = uniforms.downsampled2x > 0 ? stepPixel * 2 + pixelOffset : stepPixel;
                            vec3 rayIrradiance = texelFetch(directLightTexture, lightPixel, 0).rgb;
                            float dist = distance(viewPos, stepPos);
                            irradiance += rayIrradiance * max(NdotL, 0.0) * surface.NdotL / max(0.1, dist);
                        }
                        hit = true;

                        break;
                    }

                }

                hits += hit ? 1.0 : 0.0;
            }

            irradiance /= float(uniforms.rayCount);

            float irradianceMax = max(max(max(irradiance.r,
                max(irradiance.g, irradiance.b)), uniforms.radianceLimit), 0.01);
            irradiance *= (uniforms.radianceLimit / irradianceMax);
        }

        float ao = max(1.0 - (hits / float(uniforms.rayCount)), 0.0);

        imageStore(giImage, pixel, vec4(irradiance, ao));
    }

}