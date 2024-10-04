#extension GL_EXT_nonuniform_qualifier : require

layout (local_size_x = 16, local_size_y = 16) in;

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
layout(set = 3, binding = 2) uniform sampler2D cloudMap;
layout(set = 3, binding = 3) uniform sampler2D oceanDepthTexture;
layout(set = 3, binding = 4) uniform usampler2D oceanStencilTexture;
layout(set = 3, binding = 5) uniform sampler2D volumetricCloudTexture;
layout(set = 3, binding = 6) uniform sampler shadowSampler;

layout(std140, set = 3, binding = 7) uniform UniformBuffer {
    int sampleCount;
    float intensity;
    int fogEnabled;
    float oceanHeight;
    int lightCount;
    int offsetX;
    int offsetY;
    int directionalLightCount;
    vec4 planetCenterAndRadius;
    Fog fog;
    CloudShadow cloudShadow;
} uniforms;

layout (std430, set = 3, binding = 8) buffer VolumetricLights {
    VolumetricLight volumetricLights[];
};

layout (std430, set = 3, binding = 9) buffer VolumetricShadows {
    Shadow volumetricShadows[];
};

layout(std430, set = 3, binding = 10) buffer LightIndicesBuffer {
    int lightIndices[];
};

layout(set = 3, binding = 11) uniform texture2DArray cascadeMaps[8];
layout(set = 3, binding = 19) uniform textureCube cubeMaps[8];

const int lightCountPerTile = 63;

shared uint sharedLightIndices[64];
shared int sharedLightIndicesCount;

vec4 ComputeVolumetricDirectionalLights(vec3 fragPos, float startDepth, vec2 texCoords);
vec4 ComputeVolumetricPunctualLights(uint lightType, vec3 fragPos, float startDepth, vec2 texCoords);
float GetShadowFactorDirectionalLight(int lightIdx, int shadowIdx, uint lightType, bool isMain, vec3 currentPosition);
float GetShadowFactorPunctualLight(int lightIdx, int shadowIdx, uint lightType, bool isMain, vec3 currentPosition);

int GetLightIndicesOffset() {

    int groupOffset = int(gl_WorkGroupID.x + gl_WorkGroupID.y * gl_NumWorkGroups.x);
    return groupOffset * (lightCountPerTile + 1);

}

void LoadLightTypeData(uint requestedLightType) {

    barrier();

    int groupSize = int(gl_WorkGroupSize.x * gl_WorkGroupSize.y);
    int localOffset = int(gl_LocalInvocationIndex);

    if (localOffset == 0) {
        sharedLightIndicesCount = 0;
    }

    barrier();

    int groupOffset = GetLightIndicesOffset();
    int lightCount = min(lightCountPerTile, lightIndices[groupOffset]);

    int lastIdx = 0;
    for (int i = localOffset; i < lightCount; i += groupSize) {
        uint lightIdx = lightIndices[groupOffset + i + 1];
        uint lightType = floatBitsToUint(volumetricLights[lightIdx].color.a);
        if (requestedLightType != lightType)
            continue;

        int idx = atomicAdd(sharedLightIndicesCount, 1);
        if (idx < lightCountPerTile)
            sharedLightIndices[idx] = lightIdx;
    }

    barrier();

    if (localOffset == 0) {
        sharedLightIndicesCount = lightCount;
    }

    barrier();

}

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

    bool validPixel = pixel.x < imageSize(volumetricImage).x &&
        pixel.y < imageSize(volumetricImage).y;

    vec4 radiance = vec4(0.0);
    radiance = ComputeVolumetricDirectionalLights(endPos, startDepth, texCoord);

#ifdef LOCAL_LIGHTS
    LoadLightTypeData(POINT_LIGHT);
    if (sharedLightIndicesCount > 0)
        radiance.rgb += ComputeVolumetricPunctualLights(POINT_LIGHT, endPos, startDepth, texCoord).rgb;

    LoadLightTypeData(SPOT_LIGHT);
    if (sharedLightIndicesCount > 0)
        radiance.rgb += ComputeVolumetricPunctualLights(SPOT_LIGHT, endPos, startDepth, texCoord).rgb;
#endif

    if (validPixel)
        imageStore(volumetricImage, pixel, radiance);

}

vec4 ComputeVolumetricDirectionalLights(vec3 fragPos, float startDepth, vec2 texCoords) {

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
    
    vec2 interleavedTexCoords = (0.5 * texCoords + 0.5) * resolution;

    float noiseOffset = GetInterleavedGradientNoise(interleavedTexCoords);
    vec3 currentPosition = stepVector * (noiseOffset + startDepth);

#ifdef CLOUDS
    bool receivedCloudExtinction = false;
    float cloudExtinction = min(textureLod(volumetricCloudTexture, texCoords, 0.0).a, 1.0);
#endif

    for (int i = 0; i < uniforms.sampleCount; i++) {

        float t0 = float(i) / float(uniforms.sampleCount);
        float t1 = float(i + 1) / float(uniforms.sampleCount);

        t0 = t0 * t0;
        t1 = t1 * t1;

        float delta = t1 - t0;
        float t = t0 + delta * noiseOffset;

        currentPosition = rayDirection * t * rayLength + startDepth;
        stepLength = delta * rayLength;

        vec3 worldPosition = vec3(globalData.ivMatrix * vec4(currentPosition, 1.0));

        vec3 lightScattering = vec3(0.0);
        vec3 lightAmbient = vec3(1.0);
        
        // First all directional lights
        for(int j = 0; j < uniforms.directionalLightCount; j++) {
            VolumetricLight light = volumetricLights[j];

            uint lightType = floatBitsToUint(light.color.a);

            bool isMain = j == 0 ? true : false;
            float shadowValue = GetShadowFactorDirectionalLight(j, light.shadowIdx, lightType, isMain, currentPosition);

            vec3 lightPosition = currentPosition - 10000.0 * light.direction.xyz;
            lightPosition = vec3(globalData.ivMatrix * vec4(lightPosition, 1.0));
            float extinctionToLight = ComputeVolumetricFog(uniforms.fog, worldPosition, lightPosition);
            float NdotL = dot(normalize(rayDirection), normalize(light.direction.xyz));

            float phaseFunction = uniforms.fogEnabled > 0 ?
                ComputeScattering(uniforms.fog.scatteringAnisotropy, NdotL) : 1.0;

            lightScattering += shadowValue * phaseFunction * light.color.rgb * light.intensity * extinctionToLight;
        }

#ifdef CLOUDS
        vec3 planetCenter = uniforms.planetCenterAndRadius.xyz;
        float cloudInnerRadius = uniforms.planetCenterAndRadius.w;

        float distToPlanetCenter = distance(worldPosition, planetCenter);
#endif

        float density = uniforms.fog.density * GetVolumetricFogDensity(uniforms.fog, worldPosition);

        vec3 scatteringCoefficient = uniforms.fog.scatteringFactor *
            uniforms.fog.extinctionCoefficients.rgb * density;
        vec4 extinctionCoefficient = uniforms.fog.extinctionFactor *
            uniforms.fog.extinctionCoefficients * density;

        vec4 clampedExtinction = max(extinctionCoefficient, 0.0000001);
        vec4 stepExtinction = exp(-extinctionCoefficient * stepLength);

        vec3 stepScattering = scatteringCoefficient * (lightScattering + vec3(uniforms.fog.ambientFactor) * lightAmbient);

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

vec4 ComputeVolumetricPunctualLights(uint lightType, vec3 fragPos, float startDepth, vec2 texCoords) {

    vec2 resolution = vec2(imageSize(volumetricImage));
    vec3 viewPosition = vec3(globalData.ivMatrix * vec4(fragPos, 1.0));

    // We compute this in view space
    vec3 rayVector = fragPos;
    float rayLength = clamp(length(rayVector) - startDepth, 0.0, 100.0);
    vec3 rayDirection = normalize(rayVector);
    float stepLength = rayLength / float(uniforms.sampleCount);
    vec3 stepVector = rayDirection * stepLength;
 
    vec3 foginess = vec3(0.0);
    vec4 extinction = vec4(1.0);
    vec4 extinctionWithClouds = vec4(1.0);
    
    vec2 interleavedTexCoords = (0.5 * texCoords + 0.5) * resolution;

    float noiseOffset = GetInterleavedGradientNoise(interleavedTexCoords);
    vec3 currentPosition = stepVector * (noiseOffset + startDepth);

#ifdef CLOUDS
    bool receivedCloudExtinction = false;
    float cloudExtinction = min(textureLod(volumetricCloudTexture, texCoords, 0.0).a, 1.0);
#endif

    for (int i = 0; i < uniforms.sampleCount; i++) {

        float t0 = float(i) / float(uniforms.sampleCount);
        float t1 = float(i + 1) / float(uniforms.sampleCount);

        t0 = t0 * t0;
        t1 = t1 * t1;

        float delta = t1 - t0;
        float t = t0 + delta * noiseOffset;

        //currentPosition = rayDirection * t * rayLength + startDepth;
        //stepLength = delta * rayLength;

        vec3 worldPosition = vec3(globalData.ivMatrix * vec4(currentPosition, 1.0));

        vec3 lightScattering = vec3(0.0);

        // Then all punctual lights
        for(int j = 0; j < sharedLightIndicesCount; j++) {
            int lightIdx = int(sharedLightIndices[j]);
            VolumetricLight light = volumetricLights[lightIdx];

            float NdotL = 1.0;
            float attenuation = 1.0;
            float sqrDistance = 1.0;

            float shadowValue = 1.0;

            float radius = light.direction.w;
            if (lightType == POINT_LIGHT) {
                vec3 pointToLight = light.location.xyz - currentPosition;
                sqrDistance = max(0.0, dot(pointToLight, pointToLight));

                attenuation = saturate(1.0 - pow(sqrDistance / (radius * radius), 4.0));
                if (attenuation == 0.0)
                    continue;

                NdotL = dot(-normalize(light.location.xyz), rayDirection);
                shadowValue = GetShadowFactorPunctualLight(lightIdx, light.shadowIdx, POINT_LIGHT, false, currentPosition);
            }
            else if (lightType == SPOT_LIGHT) {
                vec3 pointToLight = light.location.xyz - currentPosition;
                sqrDistance = dot(pointToLight, pointToLight);

                vec3 L = pointToLight / sqrt(sqrDistance);

                float strength = dot(L, normalize(-light.direction.xyz));
                float angleAttenuation = saturate(strength * light.specific0 + light.specific1);
                float distAttenuation = saturate(1.0 - pow(sqrDistance / (radius * radius), 4.0));
                if (angleAttenuation == 0.0 || distAttenuation == 0.0)
                    continue;

                NdotL = dot(-normalize(light.location.xyz), rayDirection);
                attenuation = min(distAttenuation * sqr(angleAttenuation), 1.0);
                shadowValue = GetShadowFactorPunctualLight(lightIdx, light.shadowIdx, SPOT_LIGHT, false, currentPosition);
            }   

            // No shadows for now
            // shadowValue = 1.0;

            float phaseFunction = uniforms.fogEnabled > 0 ?
                ComputeScattering(uniforms.fog.scatteringAnisotropy, NdotL) : 1.0;

            lightScattering += phaseFunction * shadowValue * light.color.rgb * attenuation * light.intensity / max(sqrDistance, 0.1);
        }

#ifdef CLOUDS
        vec3 planetCenter = uniforms.planetCenterAndRadius.xyz;
        float cloudInnerRadius = uniforms.planetCenterAndRadius.w;

        float distToPlanetCenter = distance(worldPosition, planetCenter);
#endif

        float density = uniforms.fog.density * GetVolumetricFogDensity(uniforms.fog, worldPosition);

        vec3 scatteringCoefficient = uniforms.fog.scatteringFactor *
            uniforms.fog.extinctionCoefficients.rgb * density;
        vec4 extinctionCoefficient = uniforms.fog.extinctionFactor *
            uniforms.fog.extinctionCoefficients * density;

        vec4 clampedExtinction = max(extinctionCoefficient, 0.0000001);
        vec4 stepExtinction = exp(-extinctionCoefficient * stepLength);

        vec3 stepScattering = scatteringCoefficient * lightScattering;

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

float GetShadowFactorDirectionalLight(int lightIdx, int shadowIdx, uint lightType, bool isMain, vec3 currentPosition) {

    float dist = -currentPosition.z;

#ifndef AE_BINDLESS
    int mapIdx = lightIdx;
#else
    int mapIdx = volumetricShadows[shadowIdx].mapIdx;
#endif

    if (shadowIdx < 0 || dist > volumetricShadows[shadowIdx].distance || mapIdx < 0)
        return 1.0;
    
    float shadowValue = 1.0;
    

        int cascadeIndex = 0;
        cascadeIndex = dist >= volumetricShadows[shadowIdx].cascades[0].distance ? 1 : cascadeIndex;
        cascadeIndex = dist >= volumetricShadows[shadowIdx].cascades[1].distance ? 2 : cascadeIndex;
        cascadeIndex = dist >= volumetricShadows[shadowIdx].cascades[2].distance ? 3 : cascadeIndex;
        cascadeIndex = dist >= volumetricShadows[shadowIdx].cascades[3].distance ? 4 : cascadeIndex;
        cascadeIndex = dist >= volumetricShadows[shadowIdx].cascades[4].distance ? 5 : cascadeIndex;

        cascadeIndex = min(volumetricShadows[shadowIdx].cascadeCount - 1, cascadeIndex);

        mat3x4 cascadeMatrix = volumetricShadows[shadowIdx].cascades[0].cascadeSpace;
        cascadeMatrix = cascadeIndex > 0 ? volumetricShadows[shadowIdx].cascades[1].cascadeSpace : cascadeMatrix;
        cascadeMatrix = cascadeIndex > 1 ? volumetricShadows[shadowIdx].cascades[2].cascadeSpace : cascadeMatrix;
        cascadeMatrix = cascadeIndex > 2 ? volumetricShadows[shadowIdx].cascades[3].cascadeSpace : cascadeMatrix;
        cascadeMatrix = cascadeIndex > 3 ? volumetricShadows[shadowIdx].cascades[4].cascadeSpace : cascadeMatrix;
        cascadeMatrix = cascadeIndex > 4 ? volumetricShadows[shadowIdx].cascades[5].cascadeSpace : cascadeMatrix;

        float positionLength = dist;
        float positionRatio = positionLength / volumetricShadows[shadowIdx].distance;
        vec3 shadowPosition = positionRatio <= 1.0 ? currentPosition : currentPosition / positionRatio;
        vec4 cascadeSpace = mat4(transpose(cascadeMatrix)) * vec4(currentPosition, 1.0);
        cascadeSpace.xyz /= cascadeSpace.w;

        cascadeSpace.xy = cascadeSpace.xy * 0.5 + 0.5;

#ifdef AE_BINDLESS
        shadowValue = texture(sampler2DArrayShadow(bindlessTextureArrays[nonuniformEXT(mapIdx)], shadowSampler),
            vec4(cascadeSpace.xy, cascadeIndex, cascadeSpace.z));
#else
        shadowValue = texture(sampler2DArrayShadow(cascadeMaps[nonuniformEXT(mapIdx)], shadowSampler),
            vec4(cascadeSpace.xy, cascadeIndex, cascadeSpace.z));
#endif

        vec3 worldPosition = vec3(globalData.ivMatrix * vec4(currentPosition, 1.0));

#ifdef CLOUD_SHADOWS
        if (isMain) {
            vec3 planetCenter = uniforms.planetCenterAndRadius.xyz;
            float cloudInnerRadius = uniforms.planetCenterAndRadius.w;
            float distToPlanetCenter = distance(worldPosition, planetCenter);
            float cloudShadowValue = CalculateCloudShadow(currentPosition, uniforms.cloudShadow, cloudMap);
            cloudShadowValue = distToPlanetCenter < cloudInnerRadius ? cloudShadowValue : 1.0;
            shadowValue = min(shadowValue, cloudShadowValue);
        }
#endif

    return min(shadowValue, 1.0);

}

float GetShadowFactorPunctualLight(int lightIdx, int shadowIdx, uint lightType, bool isMain, vec3 currentPosition) {

    float dist = -currentPosition.z;

#ifndef AE_BINDLESS
    int mapIdx = lightIdx;
#else
    int mapIdx = volumetricShadows[shadowIdx].mapIdx;
#endif

    if (shadowIdx < 0 || dist > volumetricShadows[shadowIdx].distance || mapIdx < 0)
        return 1.0;
    
    float shadowValue = 1.0;
    if (lightType == POINT_LIGHT) {
#ifdef AE_BINDLESS
        mat4 projectionMatrix;
        projectionMatrix[0] = volumetricShadows[shadowIdx].cascades[1].cascadeSpace[0];
        projectionMatrix[1] = volumetricShadows[shadowIdx].cascades[1].cascadeSpace[1];
        projectionMatrix[2] = volumetricShadows[shadowIdx].cascades[1].cascadeSpace[2];
        projectionMatrix[3] = volumetricShadows[shadowIdx].cascades[2].cascadeSpace[0];

        vec4 shadowCoords = mat4(transpose(volumetricShadows[shadowIdx].cascades[0].cascadeSpace)) * vec4(currentPosition, 1.0);
        shadowCoords.y *= -1.0;
        vec4 absPosition = abs(shadowCoords);
        float depth = -max(absPosition.x, max(absPosition.y, absPosition.z));
        vec4 clip = projectionMatrix * vec4(0.0, 0.0, depth, 1.0);    
        depth = clip.z / clip.w;

        shadowValue = clamp(texture(samplerCubeShadow(bindlessCubemaps[nonuniformEXT(mapIdx)], shadowSampler), 
            vec4(shadowCoords.xyz , depth - 0.0001)), 0.0, 1.0);
#endif
    }
    else if (lightType == SPOT_LIGHT) {
#ifdef AE_BINDLESS
        mat4 shadowMatrix;
        shadowMatrix[0] = volumetricShadows[shadowIdx].cascades[0].cascadeSpace[0];
        shadowMatrix[1] = volumetricShadows[shadowIdx].cascades[0].cascadeSpace[1];
        shadowMatrix[2] = volumetricShadows[shadowIdx].cascades[0].cascadeSpace[2];
        shadowMatrix[3] = volumetricShadows[shadowIdx].cascades[1].cascadeSpace[0];

        vec4 cascadeSpace = shadowMatrix * vec4(currentPosition, 1.0);
        cascadeSpace.xyz /= cascadeSpace.w;

        cascadeSpace.xy = cascadeSpace.xy * 0.5 + 0.5;

        shadowValue = texture(sampler2DArrayShadow(bindlessTextureArrays[nonuniformEXT(mapIdx)], shadowSampler),
            vec4(cascadeSpace.xy, 0.0, cascadeSpace.z));
#endif
    }

    return min(shadowValue, 1.0);

}