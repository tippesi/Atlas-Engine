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
#include <waterBody.hsh>

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
#ifdef REFLECTION
layout (set = 3, binding = 3) uniform sampler2D reflectionTexture;
#endif
layout (set = 3, binding = 4) uniform sampler2D refractionTexture;
layout (set = 3, binding = 5) uniform sampler2D depthTexture;
layout (set = 3, binding = 7) uniform sampler2D volumetricTexture;
layout (set = 3, binding = 8) uniform sampler2DArrayShadow cascadeMaps;
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

// From right below, the surface stays partly transparent near the camera
const float nearSurfaceTransmission = 0.6;
const float nearSurfaceCameraDepth = 1.5;
const float nearSurfaceDistance = 15.0;

// Shore softness (lower is softer)
const float shoreSoftness = 7.5;

const float fadeoutDistance = 100.0;
const float fadeoutFalloff = 0.2;

float GetPixelDepth(ivec2 pixelOffset) {

    ivec2 resolution = textureSize(oceanDepthTexture, 0);
    ivec2 pixel = ivec2(gl_FragCoord.xy);

    return texelFetch(oceanDepthTexture, pixel + pixelOffset, 0).r;
}

void main() {

    Light light = LightUniforms.light;
    
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

    vec3 norm = GetOceanRippleNormal(fNormal, fOriginalCoord.xz, shoreScaling);

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
    // The sun is a disk, the half vectors which reflect parts of it spread by half its angular radius
    float sunAlpha = sqrt(sqr(alpha) + sqr(tan(0.5 * Uniforms.sunAngularRadius)));
    vec3 halfway = normalize(lightDir - eyeDir);
    float specularFactor = shadowFactor * saturate(nDotL) * DistributionGGX(saturate(dot(norm, halfway)), sunAlpha) *
        VisibilitySmithGGXCorrelated(saturate(nDotL), max(nDotE, 0.0001), sunAlpha) *
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
#ifdef REFLECTION
    // Traced and filtered from the ocean surface together with the reflections of the scene
    vec3 reflectionColor = textureLod(reflectionTexture, ndcCoord, 0.0).rgb;
#else
    float reflectionMipLevel = surfaceRoughness * float(textureQueryLevels(specularProbe) - 1);
    vec3 reflectionColor = textureLod(specularProbe, reflectionVec, reflectionMipLevel).rgb;
#endif

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

    // Light scattered by the water body along the refracted view ray, like seen from under water
    vec3 extinction = GetWaterExtinction();
    float waterRayLength = distance(fPosition, depthPos);
    vec3 sunScattering, ambientScattering;
    GetWaterScattering(extinction, refract(eyeDir, norm, 1.0 / waterRefractiveIndex), 0.0, waterRayLength,
        light, indirectLight, sunScattering, ambientScattering);
    vec3 waterColor = shadowFactor * sunScattering + ambientScattering;
    
    // Water edges at shore sould be soft
    fresnel *= min(1.0, waterViewDepth * shoreSoftness);

    // The ground was lit with the light above the water, it lost energy on the way down
    vec3 groundPos = vec3(globalData.ivMatrix * vec4(depthPos, 1.0));
    refractionColor *= GetWaterDownwellingTransmittance(light, max(Uniforms.translation.y - groundPos.y, 0.0));
    refractionColor = refractionColor * exp(-extinction * waterRayLength) + waterColor;

    vec2 shoreInteraction = vec2(0.0);
#ifdef TERRAIN
    shoreInteraction = CalculateShoreInteraction(fOriginalCoord);
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

    // Foam is a bright diffuse surface lit by the sun and the sky
    const float foamAlbedo = 0.9;
    vec3 foamLight = max(0.0, nDotL) * shadowFactor * light.intensity * light.color.rgb * INV_PI + indirectLight;

    // Without a foam texture the foam is broken up by noise
#ifdef FOAM_TEXTURE
    float foamPattern = texture(foamTexture, fOriginalCoord.xz / (2.0 * Uniforms.foamSize), globalData.mipLodBias).r;
#else
    float foamNoise = 0.6 * texture(perlinNoiseMap, fOriginalCoord.xz / (3.0 * Uniforms.foamSize), globalData.mipLodBias).r +
        0.4 * texture(perlinNoiseMap, fOriginalCoord.xz / Uniforms.foamSize, globalData.mipLodBias).r;
    float foamPattern = saturate((foamNoise - 0.2) / 0.6);
#endif
    // The pattern shapes the foam edges and gives the foam its structure
    vec3 foamColor = foamAlbedo * mix(0.35, 1.0, foamPattern) * foamLight;
    float foamEdge = max(fwidth(foamPattern), 0.1);
    float foamCoverage = smoothstep(1.0 - foam - foamEdge, 1.0 - foam + foamEdge, foamPattern) * step(0.001, foam);
    color = mix(color, foamColor, foamCoverage);

    vec3 breakingColor = foamAlbedo * foamLight;
    color = mix(color, breakingColor, shoreInteraction.y);

    StencilFeatures features = CreateStencilFeatures();

    if (!gl_FrontFacing) {
        disturbance = (mat3(globalData.vMatrix) * -vec3(norm.x, 0.0, norm.z)).xz;

        refractionDisturbance = vec2(disturbance.x, -disturbance.y) * 0.1;
        refractionDisturbance *= min(2.0, waterViewDepth);
        vec3 transmittedColor = textureLod(refractionTexture, ndcCoord + refractionDisturbance, 0).rgb;

        // Light from above only gets through inside of Snell's window, outside of it the water body is reflected
        // The waves only bend the edge of the window slightly, it stays a circle
        vec3 windowNormal = normalize(mix(vec3(0.0, 1.0, 0.0), norm, 0.3));
        vec3 transmittedDir = refract(eyeDir, -windowNormal, waterRefractiveIndex);
        float windowTransmission = dot(transmittedDir, transmittedDir) > 0.0 ?
            1.0 - FresnelSchlick(vec3(0.02), 1.0, saturate(dot(transmittedDir, windowNormal))).r : 0.0;

        // The whole sky is squeezed into the window, what isn't on screen comes from the sky probe
        vec4 windowClip = globalData.pMatrix * globalData.vMatrix * vec4(transmittedDir, 0.0);
        vec2 windowCoord = 0.5 * windowClip.xy / max(windowClip.w, 1e-4) + 0.5;
        vec2 windowEdge = saturate(min(windowCoord, 1.0 - windowCoord) * 10.0);
        float windowOnScreen = windowClip.w > 0.0 ? windowEdge.x * windowEdge.y : 0.0;
        vec3 windowColor = mix(textureLod(specularProbe, transmittedDir, 0.0).rgb,
            textureLod(refractionTexture, windowCoord, 0.0).rgb, windowOnScreen);

        vec3 skyRadiance = textureLod(diffuseProbe, vec3(0.0, 1.0, 0.0), 0.0).rgb;
        vec3 sunScattering, ambientScattering;
        GetWaterScattering(GetWaterExtinction(), reflect(eyeDir, -norm), 0.0, 1000.0, light, skyRadiance,
            sunScattering, ambientScattering);

        color = mix(sunScattering + ambientScattering, windowColor, windowTransmission);

        // Right below the surface the outside stays visible through the waves
        float cameraDepth = Uniforms.translation.y - globalData.cameraLocation.y;
        float nearTransmission = nearSurfaceTransmission * (1.0 - smoothstep(0.0, nearSurfaceCameraDepth, cameraDepth)) *
            (1.0 - smoothstep(0.0, nearSurfaceDistance, length(fPosition)));
        color = mix(color, transmittedColor, nearTransmission * (1.0 - windowTransmission));
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
        color = ApplyVolumetrics(color, volumetricFog, volumetricClouds,
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