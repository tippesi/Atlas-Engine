#extension GL_EXT_nonuniform_qualifier : require

#define SHADOW_FILTER_VOGEL
#define SHADOW_CASCADE_BLENDING

#include <deferred.hsh>

#include <../structures.hsh>
#include <../shadow.hsh>
#include <../globals.hsh>

#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../brdf/brdfEval.hsh>
#include <../common/octahedron.hsh>
#include <../clouds/shadow.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(set = 3, binding = 0, rgba16f) uniform image2D image;

#ifdef SCREEN_SPACE_SHADOWS
layout(set = 3, binding = 1) uniform sampler2D sssTexture;
#endif
#ifdef CLOUD_SHADOWS
layout(set = 3, binding = 2) uniform sampler2D cloudMap;
#endif

layout(std140, set = 3, binding = 3) uniform UniformBuffer {
    int mapIndices[16];
    int lightCount;
} uniforms;

layout(std140, set = 3, binding = 4) uniform CloudShadowUniformBuffer {
    CloudShadow cloudShadow;
} cloudShadowUniforms;

layout(set = 3, binding = 5) uniform sampler shadowSampler;

layout(set = 3, binding = 6) uniform texture2DArray cascadeMaps[8];
layout(set = 3, binding = 14) uniform textureCube cubeMaps[8];

vec3 EvaluateLight(Light light, Surface surface, vec3 geometryNormal, bool isMain);
float GetShadowFactor(Light light, Surface surface, uint lightType, vec3 geometryNormal, bool isMain);

void main() {

    ivec2 resolution = imageSize(image);
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(resolution);

    float depth = texelFetch(depthTexture, pixel, 0).r;

    vec3 direct = imageLoad(image, pixel).rgb;
    if (depth < 1.0) {
        vec3 geometryNormal;
        // We don't have any light direction, that's why we use vec3(0.0, -1.0, 0.0) as a placeholder
        Surface surface = GetSurface(texCoord, depth, vec3(0.0, -1.0, 0.0), geometryNormal);

        for (int i = 0; i < uniforms.lightCount && i < 8; i++) {
            Light light = lights[i];

            light.shadow.mapIdx = uniforms.mapIndices[i];
            bool isMain = i == 0 ? true : false;
            direct += EvaluateLight(light, surface, geometryNormal, isMain);
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

    if (lightType == DIRECTIONAL_LIGHT) {
        surface.L = normalize(-light.direction.xyz);
    }
    else if (lightType == POINT_LIGHT) {
        vec3 pointToLight = light.location.xyz - surface.P;
        float sqrDistance = dot(pointToLight, pointToLight);
        float dist = sqrt(sqrDistance);
        lightMultiplier = pow(max(light.radius - dist, 0.0001) / light.radius, light.attenuation);

        surface.L = pointToLight / dist;
    }

    UpdateSurface(surface);

    // Direct diffuse + specular BRDF
    vec3 directDiffuse = EvaluateDiffuseBRDF(surface);
    vec3 directSpecular = EvaluateSpecularBRDF(surface);

    vec3 direct = directDiffuse + directSpecular;

    float shadowFactor = GetShadowFactor(light, surface, lightType, geometryNormal, isMain);

    vec3 radiance = light.color.rgb * light.intensity;
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
        /*
        shadowFactor = CalculateCascadedShadow(light.shadow, cascadeMaps[nonuniformEXT(light.shadow.mapIdx)], 
            shadowSampler, surface.P, vec3(vec2(pixel) + 0.5, 0.0),
            shadowNormal, saturate(dot(-light.direction.xyz, shadowNormal)));
            */
    }
    else if (lightType == POINT_LIGHT) {
        
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