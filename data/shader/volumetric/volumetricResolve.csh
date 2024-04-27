#include <../globals.hsh>
#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../common/flatten.hsh>

#include <fog.hsh>
#include <volumetric.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(set = 3, binding = 0, rgba16f) uniform image2D resolveImage;
layout(set = 3, binding = 1) uniform sampler2D lowResVolumetricTexture;
layout(set = 3, binding = 2) uniform sampler2D lowResDepthTexture;
layout(set = 3, binding = 4) uniform sampler2D depthTexture;

#ifdef CLOUDS
layout(set = 3, binding = 3) uniform sampler2D lowResVolumetricCloudsTexture;
#endif

layout(set = 3, binding = 5) uniform  UniformBuffer {
    Fog fog;
    vec4 planetCenter;
    int downsampled2x;
    int cloudsEnabled;
    int fogEnabled;
    float innerCloudRadius;
    float planetRadius;
    float cloudDistanceLimit;
} uniforms;

// (localSize / 2 + 2)^2
shared float depths[36];
shared vec4 volumetrics[36];
shared vec4 clouds[36];

const uint depthDataSize = (gl_WorkGroupSize.x / 2 + 2) * (gl_WorkGroupSize.y / 2 + 2);
const ivec2 unflattenedDepthDataSize = ivec2(gl_WorkGroupSize) / 2 + 2;

vec3 planetCenter = uniforms.planetCenter.xyz;

void LoadGroupSharedData() {

    ivec2 workGroupOffset = ivec2(gl_WorkGroupID) * ivec2(gl_WorkGroupSize) / 2 - ivec2(1);

    // We assume data size is smaller than gl_WorkGroupSize.x + gl_WorkGroupSize.y
    if (gl_LocalInvocationIndex < depthDataSize) {
        ivec2 offset = Unflatten2D(int(gl_LocalInvocationIndex), unflattenedDepthDataSize);
        offset += workGroupOffset;
        offset = clamp(offset, ivec2(0), textureSize(lowResDepthTexture, 0));
        depths[gl_LocalInvocationIndex] = texelFetch(lowResDepthTexture, offset, 0).r;
        volumetrics[gl_LocalInvocationIndex] = texelFetch(lowResVolumetricTexture, offset, 0);
#ifdef CLOUDS
        clouds[gl_LocalInvocationIndex] = texelFetch(lowResVolumetricCloudsTexture, offset, 0);
#endif
    }

    barrier();

}

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

int NearestDepth(float referenceDepth, float[9] depthVec) {

    int idx = 0;
    float nearest = distance(referenceDepth, depthVec[0]);
    for (int i = 1; i < 9; i++) {
        float dist = distance(referenceDepth, depthVec[i]);
        if (dist < nearest) {
            nearest = dist;
            idx = i;
        }
    }
    return idx;

}

void Upsample2x(float referenceDepth, vec2 texCoord, out vec4 volumetric, out vec4 volumetricClouds) {

    ivec2 pixel = ivec2(gl_LocalInvocationID) / 2 + ivec2(1);

    float invocationDepths[9];

    float minWeight = 1.0;

    referenceDepth = ConvertDepthToViewSpaceDepth(referenceDepth);

    for (uint i = 0; i < 9; i++) {
        int sharedMemoryOffset = Flatten2D(pixel + offsets[i], unflattenedDepthDataSize);

        float depth = ConvertDepthToViewSpaceDepth(depths[sharedMemoryOffset]);

        float depthDiff = abs(referenceDepth - depth);
        float depthWeight = min(exp(-depthDiff), 1.0);
        minWeight = min(minWeight, depthWeight);

        invocationDepths[i] = depth;
    }

    int idx = NearestDepth(referenceDepth, invocationDepths);
    int offset = Flatten2D(pixel + offsets[idx], unflattenedDepthDataSize);

    volumetric = volumetrics[offset];
#ifdef CLOUDS
    vec4 bilinearCloudScattering = texture(lowResVolumetricCloudsTexture, texCoord);
    volumetricClouds = mix(clouds[offset], bilinearCloudScattering, minWeight);
#endif
}

void main() {

    if (uniforms.downsampled2x > 0) LoadGroupSharedData();

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(resolveImage).x ||
        pixel.y > imageSize(resolveImage).y)
        return;

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(imageSize(resolveImage));

    float depth = texelFetch(depthTexture, pixel, 0).r;

    vec4 volumetricFog;
    vec4 volumetricClouds = vec4(0.0);
    if (uniforms.downsampled2x > 0) {
        Upsample2x(depth, texCoord, volumetricFog, volumetricClouds);
    }
    else {
        volumetricFog = textureLod(lowResVolumetricTexture, texCoord, 0);
#ifdef CLOUDS
        volumetricClouds = textureLod(lowResVolumetricCloudsTexture, texCoord, 0);
#endif
    }

    vec3 viewPosition = ConvertDepthToViewSpace(depth, texCoord);

    vec3 worldDirection = normalize(vec3(globalData.ivMatrix * vec4(viewPosition, 0.0)));

    vec3 resolve = imageLoad(resolveImage, pixel).rgb;

#ifndef RAYMARCHED_FOG
    vec3 worldPosition = vec3(globalData.ivMatrix * vec4(viewPosition, 1.0));
    volumetricFog.a = ComputeVolumetricFog(uniforms.fog, globalData.cameraLocation.xyz, worldPosition);

    volumetricFog.rgb = uniforms.fog.extinctionCoefficients.rgb * clamp(1.0 - volumetricFog.a, 0.0, 1.0);
#endif

    resolve = ApplyVolumetrics(uniforms.fog, resolve, volumetricFog, volumetricClouds,
        worldDirection, planetCenter, uniforms.innerCloudRadius);

    imageStore(resolveImage, pixel, vec4(resolve, 1.0));

}