#include <../globals.hsh>
#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../common/flatten.hsh>

#include <fog.hsh>

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
    int downsampled2x;
    int cloudsEnabled;
    int fogEnabled;
    float innerCloudRadius;
    float planetRadius;
    float cloudDistanceLimit;
} uniforms;

// (localSize / 2 + 2)^2
shared float depths[36];
shared vec3 volumetrics[36];
shared vec4 clouds[36];

const uint depthDataSize = (gl_WorkGroupSize.x / 2 + 2) * (gl_WorkGroupSize.y / 2 + 2);
const ivec2 unflattenedDepthDataSize = ivec2(gl_WorkGroupSize) / 2 + 2;

vec3 planetCenter = -vec3(0.0, uniforms.planetRadius, 0.0);

void LoadGroupSharedData() {

    ivec2 workGroupOffset = ivec2(gl_WorkGroupID) * ivec2(gl_WorkGroupSize) / 2 - ivec2(1);

    // We assume data size is smaller than gl_WorkGroupSize.x + gl_WorkGroupSize.y
    if (gl_LocalInvocationIndex < depthDataSize) {
        ivec2 offset = Unflatten2D(int(gl_LocalInvocationIndex), unflattenedDepthDataSize);
        offset += workGroupOffset;
        offset = clamp(offset, ivec2(0), textureSize(lowResDepthTexture, 0));
        depths[gl_LocalInvocationIndex] = texelFetch(lowResDepthTexture, offset, 0).r;
        volumetrics[gl_LocalInvocationIndex] = texelFetch(lowResVolumetricTexture, offset, 0).rgb;
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

    for (uint i = 0; i < 9; i++) {
        int sharedMemoryOffset = Flatten2D(pixel + offsets[i], unflattenedDepthDataSize);

        float depth = depths[sharedMemoryOffset];

        float depthDiff = abs(referenceDepth - depth);
        float depthWeight = min(exp(-depthDiff * 32.0), 1.0);
        minWeight = min(minWeight, depthWeight);

        invocationDepths[i] = depth;
    }

    int idx = NearestDepth(referenceDepth, invocationDepths);
    int offset = Flatten2D(pixel + offsets[idx], unflattenedDepthDataSize);

    volumetric = vec4(volumetrics[offset], 1.0);
#ifdef CLOUDS
    vec4 bilinearCloudScattering = texture(lowResVolumetricCloudsTexture, texCoord);
    volumetricClouds = mix(clouds[offset], bilinearCloudScattering, minWeight);
#endif
}

vec2 IntersectSphere(vec3 origin, vec3 direction, vec3 pos, float radius) {

    vec3 L = pos - origin;
    float DT = dot(L, direction);
    float r2 = radius * radius;

    float ct2 = dot(L, L) - DT * DT;

    if (ct2 > r2)
    return vec2(-1.0);

    float AT = sqrt(r2 - ct2);
    float BT = AT;

    float AO = DT - AT;
    float BO = DT + BT;

    float minDist = min(AO, BO);
    float maxDist = max(AO, BO);

    return vec2(minDist, maxDist);
}

void main() {

    if (uniforms.downsampled2x > 0) LoadGroupSharedData();

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(resolveImage).x ||
        pixel.y > imageSize(resolveImage).y)
        return;

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(imageSize(resolveImage));

    float depth = texelFetch(depthTexture, pixel, 0).r;

    vec4 volumetric;
    vec4 cloudScattering;
    if (uniforms.downsampled2x > 0) {
        Upsample2x(depth, texCoord, volumetric, cloudScattering);
    }
    else {
        volumetric = vec4(textureLod(lowResVolumetricTexture, texCoord, 0).rgb, 0.0);
#ifdef CLOUDS
        cloudScattering = texture(lowResVolumetricCloudsTexture, texCoord);
#endif
    }

    vec3 viewPosition = ConvertDepthToViewSpace(depth, texCoord);
    float viewLength = length(viewPosition);

    vec3 worldDirection = normalize(vec3(globalData.ivMatrix * vec4(viewPosition, 0.0)));

    vec2 intersectDists = IntersectSphere(globalData.cameraLocation.xyz, worldDirection,
        planetCenter, uniforms.innerCloudRadius);
    vec2 planetDists = IntersectSphere(globalData.cameraLocation.xyz, worldDirection,
        planetCenter, uniforms.planetRadius);

    // x => first intersection with sphere, y => second intersection with sphere
    float cloudFadeout = 1.0;
    float cloudDist = intersectDists.y;
    float planetDist = planetDists.x < 0.0 ? planetDists.y : planetDists.x;
    if (depth == 1.0) {
        float maxDist = max(cloudDist, planetDist);
        // If we don't hit at all we don't want any fog
        viewPosition *= intersectDists.y > 0.0 ? max(maxDist / viewLength, 1.0) : 0.0;

        cloudFadeout = intersectDists.x < 0.0 ? sqr(saturate((uniforms.cloudDistanceLimit
            - cloudDist) / (uniforms.cloudDistanceLimit))) : cloudFadeout;
    }

    vec3 worldPosition = vec3(globalData.ivMatrix * vec4(viewPosition, 1.0));

    vec4 resolve = imageLoad(resolveImage, pixel);

    float fogAmount = uniforms.fogEnabled > 0 ? saturate(1.0 - ComputeVolumetricFog(uniforms.fog, globalData.cameraLocation.xyz, worldPosition)) : 0.0;

#ifdef CLOUDS
    if (uniforms.cloudsEnabled > 0) {
        cloudScattering.rgb *= cloudFadeout;
        cloudScattering.a = mix(1.0, cloudScattering.a, cloudFadeout);

        float alpha = cloudScattering.a;
        fogAmount = intersectDists.x < 0.0 ? fogAmount : fogAmount * alpha;
        resolve = alpha * resolve + cloudScattering;
    }
#endif
    resolve = uniforms.fogEnabled > 0 ? mix(resolve, uniforms.fog.color, fogAmount) + volumetric : resolve + volumetric;

    imageStore(resolveImage, pixel, resolve);

}