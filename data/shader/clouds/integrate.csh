#include <../globals.hsh>
#include <../structures>
#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../common/random.hsh>
#include <../common/flatten.hsh>
#include <../common/bluenoise.hsh>
#include <../common/PI.hsh>

#include <clouds.hsh>
#include <integrate.hsh>

layout (local_size_x = 8, local_size_y = 4) in;

layout(set = 3, binding = 0, rgba16f) writeonly uniform image2D volumetricCloudImage;

layout(set = 3, binding = 5) uniform sampler2D scramblingRankingTexture;
layout(set = 3, binding = 6) uniform sampler2D sobolSequenceTexture;

vec4 blueNoiseVec = vec4(0.0);

vec4 ComputeVolumetricClouds(vec3 fragPos, float depth);

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(volumetricCloudImage).x ||
        pixel.y > imageSize(volumetricCloudImage).y)
        return;

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(imageSize(volumetricCloudImage));

    float depth = textureLod(depthTexture, texCoord, 0.0).r;
    vec3 pixelPos = ConvertDepthToViewSpace(depth, texCoord);

    int sampleIdx = int(cloudUniforms.frameSeed);
    blueNoiseVec = vec4(
            SampleBlueNoise(pixel, sampleIdx, 0, scramblingRankingTexture, sobolSequenceTexture),
            SampleBlueNoise(pixel, sampleIdx, 1, scramblingRankingTexture, sobolSequenceTexture),
            SampleBlueNoise(pixel, sampleIdx, 2, scramblingRankingTexture, sobolSequenceTexture),
            SampleBlueNoise(pixel, sampleIdx, 3, scramblingRankingTexture, sobolSequenceTexture)
            );

    vec4 scattering = ComputeVolumetricClouds(pixelPos, depth);
    imageStore(volumetricCloudImage, pixel, scattering);

}

vec4 ComputeVolumetricClouds(vec3 fragPos, float depth) {

    vec3 rayDirection = normalize(vec3(globalData[0].ivMatrix * vec4(fragPos, 0.0)));
    vec3 rayOrigin = globalData[0].cameraLocation.xyz;

    float inDist, outDist;
    CalculateRayLength(rayOrigin, rayDirection, inDist, outDist);

    if (inDist <= 0.0 && outDist <= 0.0)
        return vec4(0.0, 0.0, 0.0, 1.0);

    if (length(fragPos) < inDist && depth != 1.0)
        return vec4(0.0, 0.0, 0.0, 1.0);

    float rayLength = depth < 1.0 ? min(length(fragPos) - inDist, outDist - inDist) : outDist - inDist;

    return IntegrateVolumetricClouds(rayOrigin, rayDirection, inDist,
        rayLength, blueNoiseVec);

}