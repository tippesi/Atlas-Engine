#ifdef AE_TEXTURE_SHADOW_LOD
#extension GL_EXT_texture_shadow_lod : require
#endif

layout (local_size_x = 8, local_size_y = 8) in;

#include <../globals.hsh>
#include <../structures.hsh>
#include <../common/ign.hsh>
#include <../common/convert.hsh>
#include <../common/stencil.hsh>
#include <../common/utility.hsh>
#include <../common/random.hsh>
#include <../clouds/shadow.hsh>
#include <../common/bluenoise.hsh>

#include <fog.hsh>

layout(set = 3, binding = 0, rgba16f) writeonly uniform image2D volumetricImage;
layout(set = 3, binding = 1) uniform sampler2D depthTexture;
layout(set = 3, binding = 2) uniform sampler2DArrayShadow cascadeMaps;
layout(set = 3, binding = 3) uniform sampler2D cloudMap;
layout(set = 3, binding = 4) uniform sampler2D oceanDepthTexture;
layout(set = 3, binding = 5) uniform usampler2D oceanStencilTexture;
layout(set = 3, binding = 6) uniform sampler2D volumetricCloudTexture;
layout(set = 3, binding = 8) uniform sampler2D scramblingRankingTexture;
layout(set = 3, binding = 9) uniform sampler2D sobolSequenceTexture;

layout(std140, set = 3, binding = 7) uniform UniformBuffer {
    int sampleCount;
    float intensity;
    int fogEnabled;
    float oceanHeight;
    vec4 planetCenterAndRadius;
    Fog fog;
    Light light;
    CloudShadow cloudShadow;
} uniforms;

vec4 ComputeVolumetric(vec3 fragPos, float startDepth, vec2 texCoords);

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(volumetricImage).x ||
        pixel.y > imageSize(volumetricImage).y)
        return;

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(imageSize(volumetricImage));

    float depth = textureLod(depthTexture, texCoord, 0.0).r;
    vec3 pixelPos = ConvertDepthToViewSpace(depth, texCoord);

    vec3 endPos = pixelPos;

    float startDepth = 0.0;
#ifdef OCEAN
    StencilFeatures features = DecodeStencilFeatures(textureLod(oceanStencilTexture, texCoord, 0.0).r);

    float oceanDepth = textureLod(oceanDepthTexture, texCoord, 0.0).r;
    vec3 oceanPos = ConvertDepthToViewSpace(oceanDepth, texCoord);

    // We could use stencil features here, but they are unrealiable. Next option is to just apply
    // simpl fog to the refraction texture when under water
    if (oceanDepth < depth && !features.underWaterPixel) {
        endPos = oceanPos;
    }
#endif

    vec4 radiance = vec4(0.0);
    radiance = ComputeVolumetric(endPos, startDepth, texCoord);
    imageStore(volumetricImage, pixel, radiance);

}

vec4 ComputeVolumetric(vec3 fragPos, float startDepth, vec2 texCoords) {

    vec2 resolution = vec2(imageSize(volumetricImage));
    vec3 viewPosition = vec3(globalData.ivMatrix * vec4(fragPos, 1.0));

    // We compute this in view space
    vec3 rayVector = fragPos;
    float rayLength = max(length(rayVector) - startDepth, 0.0);
    vec3 rayDirection = rayVector / rayLength;
    float stepLength = rayLength / float(uniforms.sampleCount);
    vec3 stepVector = rayDirection * stepLength;
 
    vec3 foginess = vec3(0.0);
    vec4 extinction = vec4(1.0);
    vec4 extinctionWithClouds = vec4(1.0);
    
    vec2 interleavedTexCoords = (0.5 * texCoords + 0.5) * resolution * 4.0;

    float noiseOffset = GetInterleavedGradientNoise(interleavedTexCoords);

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    int sampleIdx = int(globalData.frameCount) % 16;
    //noiseOffset = SampleBlueNoise(pixel, sampleIdx, 0, scramblingRankingTexture, sobolSequenceTexture);

    vec3 currentPosition = stepVector * (noiseOffset + startDepth);

    int cascadeIndex = 0;
    int lastCascadeIndex = 0;
    mat4 cascadeMatrix = uniforms.light.shadow.cascades[0].cascadeSpace;

#ifdef CLOUDS
    bool receivedCloudExtinction = false;
    float cloudExtinction = min(textureLod(volumetricCloudTexture, texCoords, 0.0).a, 1.0);
#endif

    for (int i = 0; i < uniforms.sampleCount; i++) {
        
        float dist = -currentPosition.z;

        int cascadeIndex = 0;

        cascadeIndex = dist >= uniforms.light.shadow.cascades[0].distance ? 1 : cascadeIndex;
        cascadeIndex = dist >= uniforms.light.shadow.cascades[1].distance ? 2 : cascadeIndex;
        cascadeIndex = dist >= uniforms.light.shadow.cascades[2].distance ? 3 : cascadeIndex;
        cascadeIndex = dist >= uniforms.light.shadow.cascades[3].distance ? 4 : cascadeIndex;
        cascadeIndex = dist >= uniforms.light.shadow.cascades[4].distance ? 5 : cascadeIndex;

        cascadeIndex = min(uniforms.light.shadow.cascadeCount - 1, cascadeIndex);

        if (lastCascadeIndex != cascadeIndex) {
            cascadeMatrix = uniforms.light.shadow.cascades[0].cascadeSpace;
            cascadeMatrix = cascadeIndex > 0 ? uniforms.light.shadow.cascades[1].cascadeSpace : cascadeMatrix;
            cascadeMatrix = cascadeIndex > 1 ? uniforms.light.shadow.cascades[2].cascadeSpace : cascadeMatrix;
            cascadeMatrix = cascadeIndex > 2 ? uniforms.light.shadow.cascades[3].cascadeSpace : cascadeMatrix;
            cascadeMatrix = cascadeIndex > 3 ? uniforms.light.shadow.cascades[4].cascadeSpace : cascadeMatrix;
            cascadeMatrix = cascadeIndex > 4 ? uniforms.light.shadow.cascades[5].cascadeSpace : cascadeMatrix;
        }

        lastCascadeIndex = cascadeIndex;

        float positionLength = dist;
        float positionRatio = positionLength / uniforms.light.shadow.distance;
        vec3 shadowPosition = positionRatio <= 1.0 ? currentPosition : currentPosition / positionRatio;
        vec4 cascadeSpace = cascadeMatrix * vec4(currentPosition, 1.0);
        cascadeSpace.xyz /= cascadeSpace.w;

        cascadeSpace.xy = cascadeSpace.xy * 0.5 + 0.5;

        float shadowValue = 1.0;
#ifdef SHADOWS        
#ifdef AE_TEXTURE_SHADOW_LOD
        // This fixes issues that can occur at cascade borders
        shadowValue = textureLod(cascadeMaps,
            vec4(cascadeSpace.xy, cascadeIndex, cascadeSpace.z), 0);
#else
        shadowValue = texture(cascadeMaps,
            vec4(cascadeSpace.xy, cascadeIndex, cascadeSpace.z));
#endif
#endif

        //shadowValue = dist > uniforms.light.shadow.distance ? 1.0 : shadowValue;
        vec3 worldPosition = vec3(globalData.ivMatrix * vec4(currentPosition, 1.0));

#ifdef CLOUDS
        vec3 planetCenter = uniforms.planetCenterAndRadius.xyz;
        float cloudInnerRadius = uniforms.planetCenterAndRadius.w;

        float distToPlanetCenter = distance(worldPosition, planetCenter);
#endif

#ifdef CLOUD_SHADOWS
        float cloudShadowValue = CalculateCloudShadow(currentPosition, uniforms.cloudShadow, cloudMap);
        cloudShadowValue = distToPlanetCenter < cloudInnerRadius ? cloudShadowValue : 1.0;
        shadowValue = min(shadowValue, cloudShadowValue);
#endif

        float density = uniforms.fog.density * GetVolumetricFogDensity(uniforms.fog, worldPosition);

        vec3 scatteringCoefficient = uniforms.fog.scatteringFactor *
            uniforms.fog.extinctionCoefficients.rgb * density;
        vec4 extinctionCoefficient = uniforms.fog.extinctionFactor *
            uniforms.fog.extinctionCoefficients * density;

        float NdotL = dot(rayDirection, uniforms.light.direction.xyz);

        vec4 clampedExtinction = max(extinctionCoefficient, 0.0000001);
        vec4 stepExtinction = exp(-extinctionCoefficient * stepLength);

        float phaseFunction = uniforms.fogEnabled > 0 ?
            ComputeScattering(uniforms.fog.scatteringAnisotropy, NdotL) : 1.0;
        vec3 stepScattering = scatteringCoefficient * (shadowValue * phaseFunction * uniforms.light.color.rgb
            + vec3(uniforms.fog.ambientFactor));

        vec3 luminanceIntegral = (stepScattering - stepScattering * stepExtinction.rgb) / clampedExtinction.rgb;
#ifdef CLOUDS
        foginess += luminanceIntegral * extinctionWithClouds.rgb;

        if (distToPlanetCenter > cloudInnerRadius && !receivedCloudExtinction) {
            extinctionWithClouds *= uniforms.fogEnabled > 0 ? stepExtinction * cloudExtinction : vec4(1.0);
            receivedCloudExtinction = true;
        }
        else {
            extinctionWithClouds *= uniforms.fogEnabled > 0 ? stepExtinction : vec4(1.0);
        }
#else
        foginess += luminanceIntegral * extinction.rgb;
#endif

        extinction *= uniforms.fogEnabled > 0 ? stepExtinction : vec4(1.0);

        currentPosition += stepVector;

    }

    return vec4(foginess * uniforms.intensity, extinction.a);

}