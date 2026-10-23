#define SHADOW_FILTER_VOGEL

#include <common.hsh>
#include <surface.hsh>
#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../common/stencil.hsh>
#include <../common/normalencode.hsh>
#include <../common/normalreconstruction.hsh>
#include <../clouds/shadow.hsh>
#include <../volumetric/volumetric.hsh>
#include <../structures.hsh>
#include <../brdf/brdf.hsh>

#include <shoreInteraction.hsh>

#ifdef DDGI
#include <../ddgi/ddgi.hsh>
#endif

// From deferred.hsh
layout(set = 1, binding = 11) uniform samplerCube specularProbe;
layout(set = 1, binding = 12) uniform samplerCube diffuseProbe;

// Lighting based on Island Demo (NVIDIA SDK 11)

layout (location = 0) out vec3 color;
layout (location = 1) out vec2 velocity;
layout (location = 2) out uint stencil;

layout (set = 3, binding = 2) uniform sampler2D foamTexture;
layout (set = 3, binding = 4) uniform sampler2D refractionTexture;
layout (set = 3, binding = 5) uniform sampler2D depthTexture;
layout (set = 3, binding = 7) uniform sampler2D volumetricTexture;
layout (set = 3, binding = 8) uniform sampler2DArrayShadow cascadeMaps;
layout (set = 3, binding = 10) uniform sampler2D rippleTexture;
layout(set = 3, binding = 15) uniform sampler2D cloudShadowMap;
layout(set = 3, binding = 16) uniform sampler2D volumetricCloudTexture;
layout(set = 3, binding = 20) uniform sampler2D oceanDepthTexture;

layout(location=0) in vec4 fClipSpace;
layout(location=1) in vec3 fPosition;
layout(location=2) in vec3 fModelCoord;
layout(location=3) in vec3 fOriginalCoord;
// layout(location=5) in float waterDepth;
layout(location=6) in float shoreScaling;
layout(location=7) in vec3 ndcCurrent;
layout(location=8) in vec3 ndcLast;
#ifdef TERRAIN
layout(location=9) in vec3 normalShoreWave;
#endif

// Sun light scattered through the thin wave crests, the jacobian tells how thin they are
const float scatterIntensity = 2.5;
const float scatterThinnessScale = 4.0;
const float scatterCrestHeight = 1.0;

// Shore softness (lower is softer)
const float shoreSoftness = 70.5;

const float fadeoutDistance = 100.0;
const float fadeoutFalloff = 0.2;

float GetPixelDepth(ivec2 pixelOffset) {

    ivec2 resolution = textureSize(oceanDepthTexture, 0);
    ivec2 pixel = ivec2(gl_FragCoord.xy);

    return texelFetch(oceanDepthTexture, pixel + pixelOffset, 0).r;
}

void main() {

    Light light = LightUniforms.light;
    
    vec2 fTexCoord = fOriginalCoord.xz / Uniforms.tiling;

    float fold, jacobian, slopeVariance;
    vec2 gradientDisplacement;
    float viewDistance = distance(fOriginalCoord.xyz, globalData.cameraLocation.xyz);
    GetOceanGradientAndFold(fOriginalCoord.xz, viewDistance, fold, gradientDisplacement, jacobian, slopeVariance);

    /*
    // This stuff calculates the geometry normals from the depth buffer
    float depthCenter = GetPixelDepth(ivec2(0, 0));
    float depthLeft = GetPixelDepth(ivec2(-1, 0));
    float depthRight = GetPixelDepth(ivec2(1, 0));
    float depthTop = GetPixelDepth(ivec2(0, -1));
    float depthBottom = GetPixelDepth(ivec2(0, 1));

    ivec2 resolution = ivec2(textureSize(oceanDepthTexture, 0));
    vec3 normal = normalize(vec3(globalData.ivMatrix * vec4(ReconstructNormal(resolution, depthCenter, depthLeft, depthRight,
        depthTop, depthBottom, gl_FragCoord.xy / vec2(resolution)), 0.0)));
        */

    float tileSize = Uniforms.tiling / float(Uniforms.N);
    
    vec2 ndcCoord = 0.5 * (fClipSpace.xy / fClipSpace.w) + 0.5;
    float clipDepth = textureLod(depthTexture, ndcCoord, 0.0).r;
    
    vec3 depthPos = ConvertDepthToViewSpace(clipDepth, ndcCoord);
    //vec3 fNormal = normal;
    vec3 fNormal = normalize(vec3(gradientDisplacement.x, 1.0, gradientDisplacement.y));
    
    float shadowFactor = CalculateCascadedShadow(light.shadow, cascadeMaps, fPosition, 
        vec3(gl_FragCoord.xy, 0.0), fNormal, 0.0);


#ifdef CLOUD_SHADOWS
    float cloudShadowFactor = CalculateCloudShadow(fPosition, cloudShadowUniforms.cloudShadow, cloudShadowMap);
    shadowFactor *= cloudShadowFactor;
#endif

    shadowFactor = max(shadowFactor, 0.01);

#ifdef TERRAIN
    fNormal = mix(normalShoreWave, fNormal, shoreScaling);
#endif

    // Create TBN matrix for normal mapping
    vec3 norm = fNormal;
    vec3 tang = vec3(1.0, 0.0, 0.0);
    tang.y = -((norm.x*tang.x) / norm.y) - ((norm.z*tang.z) / norm.y);
    tang = normalize(tang);
    vec3 bitang = normalize(cross(tang, norm));
    mat3 tbn = mat3(tang, bitang, norm);
    
    // Normal mapping normal (offsets actual normal)
    vec3 rippleNormal = vec3(0.0, 1.0, 0.0);

#ifdef RIPPLE_TEXTURE
        rippleNormal = normalize(2.0 * texture(rippleTexture, 20.0 * fTexCoord - vec2(globalData.time * 0.2), globalData.mipLodBias).rgb - 1.0);
        rippleNormal += normalize(2.0 * texture(rippleTexture, 20.0 * fTexCoord * 0.5 + vec2(globalData.time * 0.05), globalData.mipLodBias).rgb - 1.0);
        // Won't work with rippleNormal = vec3(0.0, 1.0, 0.0). Might be worth an investigation
        norm = normalize(tbn * rippleNormal);
#endif
    
    // Scale ripples based on actual (not view) depth of water
    float rippleScaling = clamp(1.0 - shoreScaling, 0.05, 0.1);
    norm = normalize(mix(fNormal, norm, rippleScaling));

    vec3 eyeDir = normalize(fModelCoord - globalData.cameraLocation.xyz);
    vec3 lightDir = -light.direction.xyz;

    float nDotL = dot(norm, lightDir);
    float nDotE = saturate(dot(norm, -eyeDir));

    // Calculate fresnel factor
    float fresnel = FresnelSchlick(vec3(0.02), 1.0, nDotE).r;

    // Reflections below the horizon would show the ground of the sky probe
    vec3 reflectionVec = normalize(reflect(eyeDir, norm));
    reflectionVec.y = max(0.0, reflectionVec.y);
    reflectionVec = normalize(reflectionVec);

    // Calculate sun spot
    float alpha = GetOceanAlpha(slopeVariance);
    float surfaceRoughness = sqrt(alpha);
    vec3 halfway = normalize(lightDir - eyeDir);
    float specularFactor = shadowFactor * saturate(nDotL) * DistributionGGX(saturate(dot(norm, halfway)), alpha) *
        VisibilitySmithGGXCorrelated(saturate(nDotL), max(nDotE, 0.0001), alpha) *
        FresnelSchlick(vec3(0.02), 1.0, saturate(dot(-eyeDir, halfway))).r;

    // Scattering equations
    float waveHeight = fModelCoord.y - Uniforms.translation.y;
    float crestThinness = saturate((1.0 - jacobian) * scatterThinnessScale) *
        smoothstep(0.0, scatterCrestHeight, waveHeight);
    // Only visible when looking towards the sun through the wave side which faces away from it
    float scatterFactor = scatterIntensity * crestThinness * pow(saturate(dot(lightDir, eyeDir)), 4.0) *
        pow(saturate(0.5 - 0.5 * dot(lightDir, norm)), 3.0);
    scatterFactor *= shadowFactor * saturate(lightDir.y + 0.2);

    // Calculate water depth based on the viewer (ray from camera to ground)
    float waterViewDepth = max(0.0, fPosition.z - depthPos.z);
    
    vec2 disturbance = (mat3(globalData.vMatrix) * vec3(norm.x, 0.0, norm.z)).xz;

    vec2 refractionDisturbance = vec2(-disturbance.x, disturbance.y) * 0.02;
    refractionDisturbance *= min(2.0, waterViewDepth);

    // Retrieve colors from textures
    vec3 refractionColor = textureLod(refractionTexture, ndcCoord + refractionDisturbance, 0).rgb;
    float reflectionMipLevel = surfaceRoughness * float(textureQueryLevels(specularProbe) - 1);
    vec3 reflectionColor = textureLod(specularProbe, reflectionVec, reflectionMipLevel).rgb;

    // Calculate water color
    vec3 depthFog = mix(Uniforms.deepWaterBodyColor.rgb, Uniforms.waterBodyColor.rgb, min(1.0 , exp(-waterViewDepth / 10.0)));

    // The light which enters the water body from above, like the indirect diffuse lighting of the scene
    vec3 indirectLight = textureLod(diffuseProbe, norm, 0.0).rgb;
#ifdef DDGI
#ifdef DDGI_SCROLL
    vec4 indirectLightLocal = ddgiData.volumeEnabled > 0 ?
        GetLocalIrradiance(fModelCoord, eyeDir, norm, norm, indirectLight) : vec4(0.0, 0.0, 0.0, 1.0);
#else
    vec4 indirectLightLocal = ddgiData.volumeEnabled > 0 ?
        GetLocalIrradiance(fModelCoord, eyeDir, norm, norm) : vec4(0.0, 0.0, 0.0, 1.0);
#endif
    indirectLightLocal = IsInsideVolume(fModelCoord) ? indirectLightLocal : vec4(0.0, 0.0, 0.0, 1.0);
    indirectLight = (indirectLightLocal.rgb + indirectLight * indirectLightLocal.a) * ddgiData.volumeStrength;
#endif

    vec3 waterColor = depthFog * (Uniforms.waterColorIntensity.x * indirectLight +
        Uniforms.waterColorIntensity.y * max(0.0, nDotL) * shadowFactor * light.intensity * light.color.rgb);
    
    // Water edges at shore sould be soft
    fresnel *= min(1.0, waterViewDepth * shoreSoftness);

    // Update refraction color based on water depth (exponential falloff)
    refractionColor = mix(waterColor, refractionColor, min(1.0 , exp(-waterViewDepth / 2.0)));

    vec2 shoreInteraction = vec2(0.0);
#ifdef TERRAIN
    shoreInteraction = CalculateShoreInteraction(fModelCoord);
#endif

    // Calculate foam based on folding of wave and fade it out near shores
    float foam = fold;
    foam += shoreInteraction.x;
    foam = min(foam, 1.0);

    // Fade the reflection out caused by foam
    reflectionColor *= (1.0 - 0.5 * foam);

    // Mix relection and refraction and add sun spot
    color = mix(refractionColor, reflectionColor, fresnel);
    color += specularFactor * light.intensity * light.color.rgb;
    color += (1.0 - fresnel) * scatterFactor * light.intensity * light.color.rgb * Uniforms.scatterColor.rgb;

    vec3 foamShadowFactor = mix(vec3(0.1),
        vec3(0.5) * max(0.0, nDotL) * shadowFactor, 0.7);

    // Without a foam texture the foam is broken up by noise
    vec3 foamColor = foamShadowFactor * light.intensity * light.color.rgb;
#ifdef FOAM_TEXTURE
    float foamPattern = texture(foamTexture, fOriginalCoord.xz / 8.0, globalData.mipLodBias).r;
#else
    float foamNoise = 0.6 * texture(perlinNoiseMap, fOriginalCoord.xz / (3.0 * Uniforms.foamSize), globalData.mipLodBias).r +
        0.4 * texture(perlinNoiseMap, fOriginalCoord.xz / Uniforms.foamSize, globalData.mipLodBias).r;
    float foamPattern = saturate((foamNoise - 0.2) / 0.6);
#endif
    // Thresholding the pattern by the amount of foam keeps the foam edges sharp instead of blending a blurry pattern
    float foamEdge = max(fwidth(foamPattern), 0.02);
    float foamCoverage = smoothstep(1.0 - foam - foamEdge, 1.0 - foam + foamEdge, foamPattern) * step(0.001, foam);
    color = mix(color, foamColor, foamCoverage);

    vec3 breakingColor = foamShadowFactor * light.intensity * light.color.rgb;
    color = mix(color, breakingColor, shoreInteraction.y);

    StencilFeatures features = CreateStencilFeatures();

    if (!gl_FrontFacing) {
        fresnel = 0.02 + (1.0 - 0.02) * pow(1.0 - saturate(dot(fNormal, eyeDir)), 5.0);
        disturbance = (mat3(globalData.vMatrix) * -vec3(norm.x, 0.0, norm.z)).xz;

        refractionDisturbance = vec2(disturbance.x, -disturbance.y) * 0.1;
        refractionDisturbance *= min(2.0, waterViewDepth);
        color = textureLod(refractionTexture, ndcCoord + refractionDisturbance, 0).rgb * (1.0 - fresnel);
        features.underWaterPixel = true;
    }
    else {
        features.underWaterPixel = false;
#ifdef FOG
        vec4 volumetricFog = textureLod(volumetricTexture, ndcCoord, 0.0);
#else
        vec4 volumetricFog = vec4(0.0, 0.0, 0.0, 0.0);
#endif
        vec4 volumetricClouds = vec4(0.0, 0.0, 0.0, 1.0);
#ifdef CLOUDS
        volumetricClouds = textureLod(volumetricCloudTexture, ndcCoord, 0.0);
#endif
        color = ApplyVolumetrics(Uniforms.fog, color, volumetricFog, volumetricClouds,
            eyeDir, globalData.planetCenter.xyz, Uniforms.innerCloudRadius);
    }

    // Calculate velocity
    vec2 ndcL = ndcLast.xy / ndcLast.z;
    vec2 ndcC = ndcCurrent.xy / ndcCurrent.z;

    ndcL -= globalData.jitterLast;
    ndcC -= globalData.jitterCurrent;

    velocity = (ndcL - ndcC) * 0.5;

    features.responsivePixel = true;
    features.waterPixel = true;
    stencil = EncodeStencilFeatures(features);

}