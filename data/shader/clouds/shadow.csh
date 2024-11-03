#include <../globals.hsh>
#include <../structures.hsh>
#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../common/random.hsh>
#include <../common/flatten.hsh>
#include <../common/bluenoise.hsh>
#include <../common/PI.hsh>

#include <clouds.hsh>
#include <integrate.hsh>

layout (local_size_x = 8, local_size_y = 4) in;

layout(set = 3, binding = 0, rg16f) writeonly uniform image2D volumetricCloudShadowImage;

layout(std140, set = 3, binding = 8) uniform UniformBuffer {
    mat4 ivMatrix;
    mat4 ipMatrix;

    vec4 lightDirection;
    int shadowSampleFraction;
} uniforms;

vec2 ComputeVolumetricClouds(vec3 minDepthPos, vec3 maxDepthPos);

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(volumetricCloudShadowImage).x ||
        pixel.y > imageSize(volumetricCloudShadowImage).y)
        return;

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(imageSize(volumetricCloudShadowImage));

    vec3 minDepthPos = ConvertDepthToViewSpace(0.0, texCoord, uniforms.ipMatrix);
    vec3 maxDepthPos = ConvertDepthToViewSpace(1.0, texCoord, uniforms.ipMatrix);

    vec2 depthExtinction = ComputeVolumetricClouds(minDepthPos, maxDepthPos);
    imageStore(volumetricCloudShadowImage, pixel, vec4(depthExtinction, 0.0, 1.0));

}

vec2 ComputeVolumetricClouds(vec3 minDepthPos, vec3 maxDepthPos) {

    vec3 rayDirection = uniforms.lightDirection.xyz;
    vec3 rayOrigin = vec3(uniforms.ivMatrix * vec4(minDepthPos, 1.0));

    float inDist, outDist;
    CalculateRayLength(rayOrigin, rayDirection, inDist, outDist);

    float rayLength = outDist - inDist;
    float rayStart = inDist;

    int raySampleCount = cloudUniforms.sampleCount / uniforms.shadowSampleFraction;
    float stepLength = rayLength / float(raySampleCount);
    vec3 stepVector = rayDirection * stepLength;

    float maxDepth = abs(maxDepthPos.z - minDepthPos.z);

    float depth = maxDepth;
    float extinction = 1.0;
    vec3 rayPos = rayOrigin + rayDirection * rayStart;

    for (int i = 0; i < raySampleCount; i++) {
        if (extinction > epsilon) {
            vec2 coverageTexCoords;
            vec3 shapeTexCoords, detailTexCoords;
            CalculateTexCoords(rayPos, shapeTexCoords, detailTexCoords, coverageTexCoords);

            // Use lower lod (2.0) for shadows
            float density = saturate(SampleDensity(rayPos, shapeTexCoords, detailTexCoords,
                coverageTexCoords, vec3(1.0), 2.0, false));

            float extinctionCoefficient = cloudUniforms.extinctionFactor *
            cloudUniforms.extinctionCoefficients.a * density;
            extinction *= exp(-extinctionCoefficient * stepLength);

            float currentDepth = rayStart + float(i) * stepLength;
            depth = mix(depth, currentDepth, pow(extinction, 4.0));
        }

        rayPos += stepVector;
    }

    return vec2(extinction < 1.0 ? depth : maxDepth, extinction);

}