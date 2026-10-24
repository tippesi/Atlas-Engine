#extension GL_EXT_nonuniform_qualifier : require

#include <volumetricLights.hsh>
#include <froxel.hsh>
#include <../common/random.hsh>

layout (local_size_x = FROXEL_CLUSTER_SIZE, local_size_y = FROXEL_CLUSTER_SIZE, local_size_z = FROXEL_CLUSTER_SIZE) in;

layout(set = 3, binding = 0, rgba16f) writeonly uniform image3D froxelImage;
layout(set = 3, binding = 1) uniform sampler3D historyFroxelTexture;

// Punctual lights only contribute to the first 100 units of a view ray, same as for the ray marching
const float punctualLightDistance = 100.0;

vec3 ComputePunctualLightScattering(uint lightType, vec3 position, vec3 rayDirection) {

    vec3 lightScattering = vec3(0.0);

    for (int j = 0; j < sharedLightIndicesCount; j++) {
        VolumetricLight light = sharedLights[j];

        float NdotL = 1.0;
        float attenuation = 1.0;
        float sqrDistance = 1.0;

        float shadowValue = 1.0;

        float radius = light.direction.w;
        if (lightType == POINT_LIGHT) {
            vec3 pointToLight = light.location.xyz - position;
            sqrDistance = max(0.0, dot(pointToLight, pointToLight));

            attenuation = saturate(1.0 - pow(sqrDistance / (radius * radius), 2.0));
            if (attenuation == 0.0)
                continue;

            NdotL = dot(normalize(position - light.location.xyz), rayDirection);
            shadowValue = GetShadowFactorPunctualLight(j, POINT_LIGHT, position);
        }
        else if (lightType == SPOT_LIGHT) {
            vec3 pointToLight = light.location.xyz - position;
            sqrDistance = dot(pointToLight, pointToLight);

            vec3 L = pointToLight / sqrt(sqrDistance);

            float strength = dot(L, normalize(-light.direction.xyz));
            float angleAttenuation = saturate(strength * light.specific0 + light.specific1);
            float distAttenuation = saturate(1.0 - pow(sqrDistance / (radius * radius), 2.0));
            if (angleAttenuation == 0.0 || distAttenuation == 0.0)
                continue;

            NdotL = dot(normalize(position - light.location.xyz), rayDirection);
            attenuation = min(distAttenuation * sqr(angleAttenuation), 1.0);
            shadowValue = GetShadowFactorPunctualLight(j, SPOT_LIGHT, position);
        }

        float phaseFunction = uniforms.fogEnabled > 0 ?
            ComputeScattering(uniforms.fog.scatteringAnisotropy, NdotL) : 1.0;

        lightScattering += phaseFunction * shadowValue * light.color.rgb * attenuation * light.intensity / max(sqrDistance, 0.1);
    }

    return lightScattering;

}

// One offset for all froxels or Cranley-Patterson rotation per froxel
vec3 GetFroxelJitter(ivec3 froxel) {

    if (pushConstants.jitterMode == 0)
        return pushConstants.jitter.xyz;

    uvec3 seed = uvec3(froxel);
    vec3 rotation = vec3(floatConstruct(hash(uvec4(seed, 0u))), floatConstruct(hash(uvec4(seed, 1u))),
        floatConstruct(hash(uvec4(seed, 2u))));
    return fract(pushConstants.jitter.xyz + rotation);

}

vec3 ComputePunctualLightSamples(uint lightType, ivec3 froxel, vec3 sampleBase, vec2 volumetricResolution) {

    const vec3 sampleShift = vec3(0.8191725134, 0.6710436067, 0.5497004779);

    vec3 lightScattering = vec3(0.0);
    for (int i = 0; i < pushConstants.localLightSampleCount; i++) {
        vec3 position = GetFroxelPosition(froxel, fract(sampleBase + float(i) * sampleShift), volumetricResolution);
        if (length(position) < punctualLightDistance)
            lightScattering += ComputePunctualLightScattering(lightType, position, normalize(position));
    }

    return lightScattering;

}

vec4 GetHistory(vec3 position, vec2 volumetricResolution, out bool valid) {

    vec3 worldPosition = vec3(globalData.ivMatrix * vec4(position, 1.0));

    vec4 lastClipSpace = globalData.pvMatrixLast * vec4(worldPosition, 1.0);
    vec2 lastTexCoord = (lastClipSpace.xy / lastClipSpace.w - globalData.jitterLast) * 0.5 + 0.5;
    float lastViewDepth = -(globalData.vMatrixLast * vec4(worldPosition, 1.0)).z;

    vec2 gridScale = volumetricResolution / vec2(pushConstants.columnCount * pushConstants.tileSize);
    vec3 historyTexCoord = vec3(lastTexCoord * gridScale, GetFroxelSlice(lastViewDepth) / float(pushConstants.sliceCount));

    valid = pushConstants.resetHistory == 0 && lastClipSpace.w > 0.0 &&
        all(greaterThanEqual(historyTexCoord, vec3(0.0))) && all(lessThanEqual(historyTexCoord, vec3(1.0)));
    return textureLod(historyFroxelTexture, historyTexCoord, 0.0);

}

void main() {

    // No early return, all invocations have to reach the barriers when loading the lights
    ivec3 froxel = ivec3(gl_GlobalInvocationID);
    bool validFroxel = all(lessThan(froxel.xy, pushConstants.columnCount)) && froxel.z < pushConstants.sliceCount;

    vec2 volumetricResolution = vec2(pushConstants.resolution);

    vec3 jitter = pushConstants.directionalJitter > 0 ? GetFroxelJitter(froxel) : vec3(0.5);
    vec3 position = GetFroxelPosition(froxel, jitter, volumetricResolution);
    vec3 rayDirection = normalize(position);
    vec3 worldPosition = vec3(globalData.ivMatrix * vec4(position, 1.0));

    vec3 lightScattering = vec3(uniforms.fog.ambientFactor);
    for (int j = 0; j < uniforms.directionalLightCount; j++) {
        VolumetricLight light = volumetricLights[j];

        uint lightType = floatBitsToUint(light.color.a);

        bool isMain = j == 0 ? true : false;
        float shadowValue = GetShadowFactorDirectionalLight(j, light.shadowIdx, lightType, isMain, position);

        vec3 lightPosition = position - 100000.0 * light.direction.xyz;
        lightPosition = vec3(globalData.ivMatrix * vec4(lightPosition, 1.0));
        float extinctionToLight = ComputeVolumetricFog(uniforms.fog, worldPosition, lightPosition);
        float NdotL = dot(rayDirection, normalize(light.direction.xyz));

        float phaseFunction = uniforms.fogEnabled > 0 ?
            ComputeScattering(uniforms.fog.scatteringAnisotropy, NdotL) : 1.0;

        lightScattering += shadowValue * phaseFunction * light.color.rgb * light.intensity * extinctionToLight;
    }

    vec3 center = GetFroxelPosition(froxel, vec3(0.5), volumetricResolution);

#ifdef LOCAL_LIGHTS
    // Jittered samples of small lights in deep slices show up as pulsing
    vec3 sampleBase = pushConstants.localLightJitter > 0 ? GetFroxelJitter(froxel) : vec3(0.5);

    vec3 punctualLightScattering = vec3(0.0);
    LoadLightTypeData(POINT_LIGHT);
    if (sharedLightIndicesCount > 0)
        punctualLightScattering += ComputePunctualLightSamples(POINT_LIGHT, froxel, sampleBase, volumetricResolution);
    LoadLightTypeData(SPOT_LIGHT);
    if (sharedLightIndicesCount > 0)
        punctualLightScattering += ComputePunctualLightSamples(SPOT_LIGHT, froxel, sampleBase, volumetricResolution);
    lightScattering += punctualLightScattering / float(pushConstants.localLightSampleCount);
#endif

    float density = uniforms.fog.density * GetVolumetricFogDensity(uniforms.fog, worldPosition);
    vec4 current = vec4(lightScattering, density);

    bool validHistory;
    vec4 history = GetHistory(center, volumetricResolution, validHistory);
    // A single invalid value would stay in the history forever
    validHistory = validHistory && !any(isnan(history)) && !any(isinf(history));
    vec4 result = validHistory ? mix(history, current, pushConstants.currentWeight) : current;

    if (validFroxel)
        imageStore(froxelImage, froxel, result);

}
