#extension GL_EXT_nonuniform_qualifier : require

#define SHADOW_FILTER_VOGEL
#define SHADOW_CASCADE_BLENDING

#include <deferred.hsh>
#include <lightCulling.hsh>

#include <../structures.hsh>
#include <../shadow.hsh>
#include <../globals.hsh>

#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../brdf/brdfEval.hsh>
#include <../common/octahedron.hsh>
#include <../clouds/shadow.hsh>

layout (local_size_x = 16, local_size_y = 16) in;

layout(set = 3, binding = 0, rgba16f) uniform image2D image;

#ifdef SCREEN_SPACE_SHADOWS
layout(set = 3, binding = 1) uniform sampler2D sssTexture;
#endif
#ifdef CLOUD_SHADOWS
layout(set = 3, binding = 2) uniform sampler2D cloudMap;
#endif

layout(std140, set = 3, binding = 5) uniform CloudShadowUniformBuffer {
    CloudShadow cloudShadow;
} cloudShadowUniforms;

layout(set = 3, binding = 4) uniform sampler shadowSampler;

layout(set = 3, binding = 7) uniform texture2DArray cascadeMaps[8];
layout(set = 3, binding = 15) uniform textureCube cubeMaps[8];

layout(push_constant) uniform constants {
    int lightCount;
    int lightBucketCount;
    int padding1;
    int padding2;
    int mapIndices[16];
} pushConstants;

shared int sharedLightBuckets[lightBucketCount];

vec3 EvaluateLight(Light light, Surface surface, vec3 geometryNormal, bool isMain);
float GetShadowFactor(Light light, Surface surface, uint lightType, vec3 geometryNormal, bool isMain);

void LoadGroupSharedData() {

    int lightBucketOffset = GetLightBucketsGroupOffset();

    int localOffset = int(gl_LocalInvocationIndex);
    for (int i = localOffset; i < pushConstants.lightBucketCount; i++) {
        sharedLightBuckets[i] = lightBuckets[lightBucketOffset + i];
    }

    barrier();

}

void main() {

    LoadGroupSharedData();

    ivec2 resolution = imageSize(image);
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(resolution);

    float depth = texelFetch(depthTexture, pixel, 0).r;

    vec3 direct = imageLoad(image, pixel).rgb;
    if (depth < 1.0) {
        vec3 geometryNormal;
        // We don't have any light direction, that's why we use vec3(0.0, -1.0, 0.0) as a placeholder
        Surface surface = GetSurface(texCoord, depth, vec3(0.0, -1.0, 0.0), geometryNormal);

        direct = vec3(0.0);

        int visibleCount = 0;

        for (int i = 0; i < pushConstants.lightBucketCount; i++) {
            int lightBucket = sharedLightBuckets[i];
            if (lightBucket == 0) continue;

            for (int j = 0; j < 32; j++) {

                bool lightVisible = (lightBucket & (1 << j)) != 0;
                if (!lightVisible) continue;

                visibleCount += 1;
                Light light = lights[i * 32 + j];

#ifndef AE_BINDLESS
                light.shadow.mapIdx = pushConstants.mapIndices[i * 32 + j];
#endif
                bool isMain = i + j == 0 ? true : false;
                direct += EvaluateLight(light, surface, geometryNormal, isMain);
            }
        }


        if (dot(surface.material.emissiveColor, vec3(1.0)) > 0.01) {    
            direct += surface.material.emissiveColor;
        }
    }


    imageStore(image, pixel, vec4(direct, 1.0));

}

uint GetLightType(Light light) {

    return floatBitsToUint(light.color.a);

}

vec3 EvaluateLight(Light light, Surface surface, vec3 geometryNormal, bool isMain) {

    uint lightType = GetLightType(light);

    float lightMultiplier = 1.0;
    float radius = light.direction.w;

    if (lightType == DIRECTIONAL_LIGHT) {
        surface.L = normalize(-light.direction.xyz);
    }
    else if (lightType == POINT_LIGHT) {
        vec3 pointToLight = light.location.xyz - surface.P;
        float sqrDistance = dot(pointToLight, pointToLight);
        float dist = sqrt(sqrDistance);
        lightMultiplier = saturate(1.0 - pow(dist / radius, 4.0)) / sqrDistance;

        surface.L = pointToLight / dist;
    }
    else if (lightType == SPOT_LIGHT) {
        vec3 pointToLight = light.location.xyz - surface.P;
        float sqrDistance = dot(pointToLight, pointToLight);
        float dist = sqrt(sqrDistance);

        surface.L = pointToLight / dist;

        float strength = dot(surface.L, normalize(-light.direction.xyz));
        float angleAttenuation = saturate(strength * light.specific0 + light.specific1);
        float distAttenuation = saturate(1.0 - pow(dist / radius, 4.0)) / sqrDistance;
        lightMultiplier = distAttenuation * sqr(angleAttenuation);        
    }

    UpdateSurface(surface);

    // Direct diffuse + specular BRDF
    vec3 directDiffuse = EvaluateDiffuseBRDF(surface);
    vec3 directSpecular = EvaluateSpecularBRDF(surface);

    vec3 direct = directDiffuse + directSpecular;

    float shadowFactor = GetShadowFactor(light, surface, lightType, geometryNormal, isMain);

    vec3 radiance = light.color.rgb * light.intensity * lightMultiplier;
    direct = direct * radiance * surface.NdotL * shadowFactor;

    if (surface.material.transmissive) {
        Surface backSurface = CreateSurface(surface.V, -surface.N, surface.L, surface.material);

        float viewDependency = saturate(dot(-surface.V, surface.L));
        // viewDependency = sqr(viewDependency);

        // Direct diffuse BRDF backside
        directDiffuse = viewDependency * surface.material.transmissiveColor * EvaluateDiffuseBRDF(backSurface);
        // Need to change this back
        // direct += directDiffuse * radiance * backSurface.NdotL * shadowFactorTransmissive;
        direct += directDiffuse * radiance * backSurface.NdotL * shadowFactor;
    }

    return direct;

}

float GetShadowFactor(Light light, Surface surface, uint lightType, vec3 geometryNormal, bool isMain) {

    if (light.shadow.cascadeCount <= 0)
        return 1.0;

    ivec2 resolution = imageSize(image);
    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(resolution);

    float shadowFactor = 1.0;

    vec3 shadowNormal = surface.material.transmissive ? dot(surface.L, geometryNormal) < 0.0 ?
            -geometryNormal : geometryNormal : geometryNormal;

    if (lightType == DIRECTIONAL_LIGHT) {
#ifdef AE_BINDLESS
        shadowFactor = CalculateCascadedShadow(light.shadow, bindlessTextureArrays[nonuniformEXT(light.shadow.mapIdx)], 
            shadowSampler, surface.P, vec3(vec2(pixel) + 0.5, 0.0),
            shadowNormal, saturate(dot(-light.direction.xyz, shadowNormal)));
#else
        shadowFactor = CalculateCascadedShadow(light.shadow, cascadeMaps[nonuniformEXT(light.shadow.mapIdx)], 
            shadowSampler, surface.P, vec3(vec2(pixel) + 0.5, 0.0),
            shadowNormal, saturate(dot(-light.direction.xyz, shadowNormal)));
#endif
    }
    else if (lightType == POINT_LIGHT) {
#ifdef AE_BINDLESS
        shadowFactor = CalculatePointShadow(light.shadow, bindlessCubemaps[nonuniformEXT(light.shadow.mapIdx)],
            shadowSampler, surface.P, shadowNormal, saturate(dot(-light.direction.xyz, shadowNormal)));
#else
        shadowFactor = CalculatePointShadow(light.shadow, cubeMaps[nonuniformEXT(light.shadow.mapIdx)],
            shadowSampler, surface.P, shadowNormal, saturate(dot(-light.direction.xyz, shadowNormal)));
#endif
    }
    else if (lightType == SPOT_LIGHT) {
#ifdef AE_BINDLESS
        shadowFactor = CalculateSpotShadow(light.shadow, bindlessTextureArrays[nonuniformEXT(light.shadow.mapIdx)], 
            shadowSampler, surface.P, vec3(vec2(pixel) + 0.5, 0.0),
            shadowNormal, saturate(dot(-light.direction.xyz, shadowNormal)));
#else
        shadowFactor = CalculateSpotShadow(light.shadow, cascadeMaps[nonuniformEXT(light.shadow.mapIdx)], 
            shadowSampler, surface.P, vec3(vec2(pixel) + 0.5, 0.0),
            shadowNormal, saturate(dot(-light.direction.xyz, shadowNormal)));
#endif
    }

    if (isMain) {
#ifdef CLOUD_SHADOWS
        float cloudShadowFactor = CalculateCloudShadow(surface.P, cloudShadowUniforms.cloudShadow, cloudMap);

        shadowFactor = min(shadowFactor, cloudShadowFactor);
#endif
        float shadowFactorTransmissive = shadowFactor;
#ifdef SCREEN_SPACE_SHADOWS
        float sssFactor = textureLod(sssTexture, texCoord, 0).r;
        shadowFactor = min(sssFactor, shadowFactor);
#endif
    }

    return shadowFactor;

}