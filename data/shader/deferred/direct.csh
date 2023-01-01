#define SHADOWS
#define SHADOW_FILTER_1x1
#define SHADOW_CASCADE_BLENDING

#include <deferred.hsh>

#include <../structures>
#include <../shadow.hsh>
#include <../globals.hsh>

#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../brdf/brdfEval.hsh>
#include <../common/octahedron.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(set = 3, binding = 0, rgba16f) uniform image2D image;
layout(set = 3, binding = 2) uniform sampler2DArrayShadow cascadeMaps;

layout(set = 3, binding = 1) uniform LightBuffer {
	Light light;
} lightData;

layout(push_constant) uniform constants {
	vec4 lightDirection;
} PushConstants;

void main() {

    if (gl_GlobalInvocationID.x > imageSize(image).x ||
        gl_GlobalInvocationID.y > imageSize(image).y)
        return;

    ivec2 resolution = imageSize(image);
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(resolution);

    float depth = texelFetch(depthTexture, pixel, 0).r;

    Light light = lightData.light;

    vec3 direct = vec3(0.0);
    if (depth < 1.0) {
        vec3 geometryNormal;
        // We don't have any light direction, that's why we use vec3(0.0, -1.0, 0.0) as a placeholder
        Surface surface = GetSurface(texCoord, depth, -light.direction.xyz, geometryNormal);

        float shadowFactor = 1.0;

        // Direct diffuse + specular BRDF
        vec3 directDiffuse = EvaluateDiffuseBRDF(surface);
        vec3 directSpecular = EvaluateSpecularBRDF(surface);

        direct = directDiffuse + directSpecular;

#ifdef SHADOWS
        // Only need to test in the direction of the light and can the be used
        // for both the transmission and reflection. The inversion is only done
        // for transmissive materials
        vec3 shadowNormal = surface.material.transmissive ? dot(-light.direction.xyz, geometryNormal) < 0.0 ? 
            -geometryNormal : geometryNormal : geometryNormal;
        vec3 suv;
        shadowFactor = CalculateCascadedShadow(light.shadow, cascadeMaps, surface.P,
            shadowNormal, saturate(dot(-light.direction.xyz, shadowNormal)), suv); 
#endif

        vec3 radiance = light.color.rgb * light.intensity;
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

        if (shadowFactor < 0.01) {
            direct += 0.05 * surface.material.baseColor;
        }

        //direct = vec3(suv.z);

    }
    
    imageStore(image, pixel, vec4(direct, 0.0));

}