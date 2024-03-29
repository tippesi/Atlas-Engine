#define SHADOW_FILTER_VOGEL
#define SHADOW_CASCADE_BLENDING

#include <deferred.hsh>

#include <../structures>
#include <../shadow.hsh>
#include <../globals.hsh>

#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../brdf/brdfEval.hsh>
#include <../common/octahedron.hsh>
#include <../clouds/shadow.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(set = 3, binding = 0, rgba16f) uniform image2D image;
#ifdef SHADOWS
layout(set = 3, binding = 1) uniform sampler2DArrayShadow cascadeMaps;
#endif
#ifdef SCREEN_SPACE_SHADOWS
layout(set = 3, binding = 2) uniform sampler2D sssTexture;
#endif
#ifdef CLOUD_SHADOWS
layout(set = 3, binding = 3) uniform sampler2D cloudMap;
#endif

layout(std140, set = 3, binding = 4) uniform UniformBuffer {
    Light light;
} uniforms;

layout(std140, set = 3, binding = 5) uniform CloudShadowUniformBuffer {
    CloudShadow cloudShadow;
} cloudShadowUniforms;

void main() {

    ivec2 resolution = imageSize(image);
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(resolution);

    float depth = texelFetch(depthTexture, pixel, 0).r;

    vec3 direct = imageLoad(image, pixel).rgb;
    if (depth < 1.0) {
        vec3 geometryNormal;
        // We don't have any light direction, that's why we use vec3(0.0, -1.0, 0.0) as a placeholder
        Surface surface = GetSurface(texCoord, depth, -uniforms.light.direction.xyz, geometryNormal);

        float shadowFactor = 1.0;

        // Direct diffuse + specular BRDF
        vec3 directDiffuse = EvaluateDiffuseBRDF(surface);
        vec3 directSpecular = EvaluateSpecularBRDF(surface);

        direct = directDiffuse + directSpecular;

#ifdef SHADOWS
        // Only need to test in the direction of the light and can the be used
        // for both the transmission and reflection. The inversion is only done
        // for transmissive materials
        vec3 shadowNormal = surface.material.transmissive ? dot(-uniforms.light.direction.xyz, geometryNormal) < 0.0 ?
            -geometryNormal : geometryNormal : geometryNormal;
        shadowFactor = CalculateCascadedShadow(uniforms.light.shadow, cascadeMaps, surface.P, vec3(vec2(pixel) + 0.5, 0.0),
            shadowNormal, saturate(dot(-uniforms.light.direction.xyz, shadowNormal)));
#endif
#ifdef SCREEN_SPACE_SHADOWS
        float sssFactor = textureLod(sssTexture, texCoord, 0).r;
        shadowFactor = min(sssFactor, shadowFactor);
#endif
#ifdef CLOUD_SHADOWS
        float cloudShadowFactor = CalculateCloudShadow(surface.P, cloudShadowUniforms.cloudShadow, cloudMap);

        shadowFactor = min(shadowFactor, cloudShadowFactor);
#endif
        vec3 radiance = uniforms.light.color.rgb * uniforms.light.intensity;
        direct = direct * radiance * surface.NdotL * shadowFactor;

        if (surface.material.transmissive) {
            Surface backSurface = CreateSurface(surface.V, -surface.N, surface.L, surface.material);

            float viewDependency = saturate(dot(-surface.V, surface.L));
            viewDependency = sqr(viewDependency);

            // Direct diffuse BRDF backside
            directDiffuse = surface.material.transmissiveColor * EvaluateDiffuseBRDF(backSurface);
            direct += directDiffuse * radiance * backSurface.NdotL * shadowFactor;
        }

        if (dot(surface.material.emissiveColor, vec3(1.0)) > 0.01) {    
            direct += surface.material.emissiveColor;
        }

        //direct = cloudShadowFactor;

    }

    imageStore(image, pixel, vec4(direct, 1.0));

}