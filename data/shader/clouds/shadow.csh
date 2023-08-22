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

layout(set = 3, binding = 0, r16f) writeonly uniform image2D volumetricCloudShadowImage;

layout(std140, set = 3, binding = 8) uniform UniformBuffer {
    mat4 ivMatrix;
    mat4 ipMatrix;
} uniforms;

float ComputeVolumetricClouds(vec3 minDepthPos, vec3 maxDepthPos);

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(volumetricCloudShadowImage).x ||
        pixel.y > imageSize(volumetricCloudShadowImage).y)
        return;

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(imageSize(volumetricCloudShadowImage));

    vec3 minDepthPos = ConvertDepthToViewSpace(0.0, texCoord, uniforms.ipMatrix);
    vec3 maxDepthPos = ConvertDepthToViewSpace(1.0, texCoord, uniforms.ipMatrix);

    float depth = ComputeVolumetricClouds(minDepthPos, maxDepthPos);
    imageStore(volumetricCloudShadowImage, pixel, vec4(exp(depth)));

}

float ComputeVolumetricClouds(vec3 minDepthPos, vec3 maxDepthPos) {

    vec3 rayDirection = normalize(vec3(uniforms.ivMatrix * vec4(minDepthPos, 0.0)));
    vec3 rayOrigin = vec3(uniforms.ivMatrix * vec4(minDepthPos, 1.0));

    float inDist, outDist;
    CalculateRayLength(rayOrigin, rayDirection, inDist, outDist);

    if (inDist <= 0.0 && outDist <= 0.0)
        return 1.0;

    float rayLength = outDist - inDist;
    float rayStart = inDist;

    const int sampleCount = cloudUniforms.sampleCount;
    int raySampleCount = max(sampleCount, int((rayLength / cloudUniforms.distanceLimit) * float(sampleCount)));
    float stepLength = rayLength / float(raySampleCount);
    vec3 stepVector = rayDirection * stepLength;

    float depth = maxDepthPos.z;
    float transmittance = 1.0;
    vec3 rayPos = rayOrigin + rayDirection * rayStart;

    for (int i = 0; i < raySampleCount; i++) {
        if (transmittance > epsilon) {
            vec2 coverageTexCoords;
            vec3 shapeTexCoords, detailTexCoords;
            CalculateTexCoords(rayPos, shapeTexCoords, detailTexCoords, coverageTexCoords);

            float density = saturate(SampleDensity(rayPos, shapeTexCoords, detailTexCoords,
                coverageTexCoords, vec3(1.0), 0.0));
            transmittance *= exp(-density * stepLength);

            float currentDepth = rayStart + float(i) * stepLength;
            depth = mix(depth, currentDepth, pow(transmittance, 4.0));
        }

        rayPos += stepVector;
    }

    return depth;

}