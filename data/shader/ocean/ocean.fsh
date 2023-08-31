#define SHADOW_FILTER_3x3

#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../common/stencil.hsh>
#include <../clouds/shadow.hsh>
#include <../structures>

#include <shoreInteraction.hsh>

// Lighting based on Island Demo (NVIDIA SDK 11)

layout (location = 0) out vec3 color;
layout (location = 1) out vec2 velocity;
layout (location = 2) out uint stencil;

layout(set = 3, binding = 1) uniform sampler2D normalMap;
layout (set = 3, binding = 2) uniform sampler2D foamTexture;
layout (set = 3, binding = 3) uniform samplerCube skyEnvProbe;
layout (set = 3, binding = 4) uniform sampler2D refractionTexture;
layout (set = 3, binding = 5) uniform sampler2D depthTexture;
layout (set = 3, binding = 7) uniform sampler2D volumetricTexture;
layout (set = 3, binding = 8) uniform sampler2DArrayShadow cascadeMaps;
layout (set = 3, binding = 10) uniform sampler2D rippleTexture;
layout(set = 3, binding = 13) uniform sampler2D perlinNoiseMap;
layout(set = 3, binding = 15) uniform sampler2D cloudMap;

layout(location=0) in vec4 fClipSpace;
layout(location=1) in vec3 fPosition;
layout(location=2) in vec3 fModelCoord;
#ifdef FOAM_TEXTURE
layout(location=3) in vec3 fOriginalCoord;
#endif
layout(location=4) in vec2 fTexCoord;
// layout(location=5) in float waterDepth;
layout(location=6) in float shoreScaling;
layout(location=7) in vec3 ndcCurrent;
layout(location=8) in vec3 ndcLast;
layout(location=9) in vec3 normalShoreWave;
layout(location=10) in float perlinScale;

const vec3 waterBodyColor = pow(vec3(0.1, 1.0, 0.7), vec3(2.2));
const vec3 deepWaterBodyColor = pow(vec3(0.1,0.15, 0.5), vec3(2.2));
const vec3 scatterColor = pow(vec3(0.3,0.7,0.6), vec3(2.2));
const vec2 waterColorIntensity = pow(vec2(0.1, 0.3), vec2(2.2));

// Control water scattering at crests
const float scatterIntensity = 1.5;
const float scatterCrestScale = 0.2;
const float scatterCrestOffset = 0.0;

// Specular properties
const float specularPower = 500.0;
const float specularIntensity = 350.0;

// Shore softness (lower is softer)
const float shoreSoftness = 70.5;

const float fadeoutDistance = 100.0;
const float fadeoutFalloff = 0.2;

void main() {

    Light light = LightUniforms.light;
    
    // Retrieve precalculated normals and wave folding information
    //vec3 fNormal = normalize(2.0 * texture(normalMap, fTexCoord).rgb - 1.0);
    float fold = texture(normalMap, fTexCoord).a;

    vec2 gradientDisplacement = texture(normalMap, fTexCoord).rg;

    vec2 gradient = perlinScale * gradientDisplacement;
    float tileSize = Uniforms.tiling / float(Uniforms.N);
    vec3 fNormal = normalize(vec3(gradient.x, 2.0 * tileSize, gradient.y));
    
    vec2 ndcCoord = 0.5 * (fClipSpace.xy / fClipSpace.w) + 0.5;
    float clipDepth = textureLod(depthTexture, ndcCoord, 0.0).r;
    
    vec3 depthPos = ConvertDepthToViewSpace(clipDepth, ndcCoord);
    
    float shadowFactor = CalculateCascadedShadow(light.shadow,
        cascadeMaps, fPosition, fNormal, 1.0);

#ifdef CLOUD_SHADOWS
    float cloudShadowFactor = CalculateCloudShadow(fPosition, cloudShadowUniforms.cloudShadow, cloudMap);
    shadowFactor = min(shadowFactor, cloudShadowFactor);
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
        rippleNormal = normalize(2.0 * texture(rippleTexture, 20.0 * fTexCoord - vec2(globalData.time * 0.2)).rgb - 1.0);
        rippleNormal += normalize(2.0 * texture(rippleTexture, 20.0 * fTexCoord * 0.5 + vec2(globalData.time * 0.05)).rgb - 1.0);
        // Won't work with rippleNormal = vec3(0.0, 1.0, 0.0). Might be worth an investigation
        norm = normalize(tbn * rippleNormal);
#endif
    
    // Scale ripples based on actual (not view) depth of water
    float rippleScaling = clamp(1.0 - shoreScaling, 0.05, 0.1);
    norm = normalize(mix(fNormal, norm, rippleScaling));

    vec3 eyeDir = normalize(fModelCoord - globalData.cameraLocation.xyz);

    float nDotL = dot(norm, -light.direction.xyz);
    float nDotE = saturate(dot(norm, -eyeDir));

    // Calculate fresnel factor
    float fresnel = 0.02 + (1.0 - 0.02) * pow(1.0 - nDotE, 5.0);
    
    // Calculate reflection vector    
    vec3 reflectionVec = normalize(reflect(eyeDir, norm));
    reflectionVec.y = max(0.0, reflectionVec.y);
    
    // Calculate sun spot
    float specularFactor = shadowFactor * fresnel * pow(max(dot(normalize(reflectionVec),
         -normalize(light.direction.xyz)), 0.0), specularPower);

    // Scattering equations
    float waveHeight = fModelCoord.y - Uniforms.translation.y;
    float scatterFactor = scatterIntensity * max(0.0, waveHeight
         * scatterCrestScale + scatterCrestOffset);

    scatterFactor *= shadowFactor * pow(max(0.0, dot(normalize(vec3(-light.direction.x,
        0.0, -light.direction.z)), eyeDir)), 2.0);
    
    scatterFactor *= pow(max(0.0, 1.0 - nDotL), 8.0);

    /*
    scatterFactor += shadowFactor * waterColorIntensity.y
         * max(0.0, waveHeight) * max(0.0, nDotE) * 
         max(0.0, 1.0 + eyeDir.y) * dot(-light.direction, -eyeDir);
         */

    // Calculate water depth based on the viewer (ray from camera to ground)
    float waterViewDepth = max(0.0, fPosition.z - depthPos.z);
    
    vec2 disturbance = (mat3(globalData.vMatrix) * vec3(norm.x, 0.0, norm.z)).xz;

    vec2 refractionDisturbance = vec2(-disturbance.x, disturbance.y) * 0.02;
    refractionDisturbance *= min(2.0, waterViewDepth);

    // Retrieve colors from textures
    vec3 refractionColor = textureLod(refractionTexture, ndcCoord + refractionDisturbance, 0).rgb;
    vec3 reflectionColor = textureLod(skyEnvProbe, reflectionVec, 0).rgb;

    // Calculate water color
    vec3 depthFog = mix(deepWaterBodyColor, waterBodyColor, min(1.0 , exp(-waterViewDepth / 10.0)));
    float diffuseFactor = waterColorIntensity.x + waterColorIntensity.y * 
        max(0.0, nDotL) * shadowFactor;
    vec3 waterColor = diffuseFactor * light.intensity * light.color.rgb * depthFog;
    
    // Water edges at shore sould be soft
    fresnel *= min(1.0, waterViewDepth * shoreSoftness);

    // Update refraction color based on water depth (exponential falloff)
    refractionColor = mix(waterColor, refractionColor, min(1.0 , exp(-waterViewDepth / 2.0)));

    vec2 shoreInteraction = vec2(0.0);
#ifdef TERRAIN
    shoreInteraction = CalculateShoreInteraction(fModelCoord);
#endif

    // Calculate foam based on folding of wave and fade it out near shores
    float foam = 0.0;
    foam += shoreInteraction.x;
    foam = min(foam, 1.0);

    // Fade the reflection out caused by foam
    reflectionColor *= (1.0 - 0.5 * foam);

    // Mix relection and refraction and add sun spot
    color = mix(refractionColor, reflectionColor, fresnel);
    color += specularIntensity * fresnel * specularFactor * light.color.rgb;
    color += scatterColor * scatterFactor;

    vec3 foamColor = vec3(1.0);
#ifdef FOAM_TEXTURE
    foamColor = vec3(texture(foamTexture, fOriginalCoord.xz / 8.0).r) *
        nDotL * light.intensity;
#endif
    color = mix(color, vec3(mix(scatterColor * 0.1, vec3(1.0), 
        foamColor)), foam);

    vec3 breakingColor = mix(vec3(0.1),
        vec3(0.5) * max(0.0, nDotL) * shadowFactor, 0.7) * light.intensity * light.color.rgb;
    color = mix(color, breakingColor, shoreInteraction.y);

    /*
        if (dot(norm, -eyeDir) < 0.0) {
        float waterViewDepth = max(0.0, -fPosition.z);
        depthFog = mix(deepWaterBodyColor, waterBodyColor, min(1.0 , exp(-waterViewDepth / 10.0)));
        color = mix(depthFog, textureLod(refractionTexture, ndcCoord + refractionDisturbance, 0).rgb, min(1.0 , exp(-waterViewDepth / 2.0)));
    }
    */

    //color = vec3(mod(fTexCoord, 1.0), 0.0);
    //color = vec3(reflectionVec.y);
    //color = vec3(perlinDisplacement);

    // Calculate velocity
    vec2 ndcL = ndcLast.xy / ndcLast.z;
    vec2 ndcC = ndcCurrent.xy / ndcCurrent.z;

    ndcL -= globalData.jitterLast;
    ndcC -= globalData.jitterCurrent;

    velocity = (ndcL - ndcC) * 0.5;

    StencilFeatures features;
    features.responsivePixel = true;
    stencil = EncodeStencilFeatures(features);

}