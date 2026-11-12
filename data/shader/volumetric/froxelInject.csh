#extension GL_EXT_nonuniform_qualifier : require

#include <froxel.hsh>

// volumetricLights.hsh reads gl_WorkGroupSize, which has to be declared before
layout (local_size_x = FROXEL_CLUSTER_SIZE, local_size_y = FROXEL_CLUSTER_SIZE, local_size_z = FROXEL_CLUSTER_SIZE) in;

#include <volumetricLights.hsh>
#include <fogVolumes.hsh>
#include <../common/random.hsh>

layout(set = 3, binding = 0, rgba16f) writeonly uniform image3D froxelImage;
layout(set = 3, binding = 1) uniform sampler3D historyFroxelTexture;

#ifdef SKY_PROBE
layout(set = 1, binding = 12) uniform samplerCube diffuseProbe;
#endif

// Punctual lights only contribute to the first 100 units of a view ray
const float punctualLightDistance = 100.0;

// Integrates the inverse square falloff along the part of the view ray inside the froxel, such that the energy doesn't
// depend on the jitter. The shadow is sampled where the falloff has most of its energy
vec3 ComputePunctualLightScattering(uint lightType, vec3 rayDirection, float rayStart, float rayEnd, float sampleOffset,
    float angularRadius) {

    vec3 lightScattering = vec3(0.0);

    for (int j = 0; j < sharedLightIndicesCount; j++) {
        VolumetricLight light = sharedLights[j];

        float radius = light.direction.w;

        float closestDistance = dot(light.location.xyz, rayDirection);
        float sqrRayDistance = max(dot(light.location.xyz, light.location.xyz) - closestDistance * closestDistance, 0.0);
        if (sqrRayDistance >= radius * radius)
            continue;

        // The ray stands for a whole froxel column, a light inside of it is spread over the footprint of the column
        float footprint = angularRadius * max(closestDistance, 0.0);
        float offset = sqrt(sqrRayDistance + footprint * footprint + 0.1);

        vec3 position = rayDirection * clamp(closestDistance, rayStart, rayEnd);
        vec3 pointToLight = light.location.xyz - position;
        float sqrDistance = dot(pointToLight, pointToLight);

        float attenuation = saturate(1.0 - pow(sqrDistance / (radius * radius), 2.0));
        if (attenuation == 0.0)
            continue;

        float angleStart = atan((rayStart - closestDistance) / offset);
        float angleEnd = atan((rayEnd - closestDistance) / offset);
        float falloff = (angleEnd - angleStart) / (offset * (rayEnd - rayStart));

        vec3 shadowPosition = rayDirection * (closestDistance + offset * tan(mix(angleStart, angleEnd, sampleOffset)));

        if (lightType == SPOT_LIGHT) {
            vec3 L = normalize(light.location.xyz - shadowPosition);
            float strength = dot(L, normalize(-light.direction.xyz));
            float angleAttenuation = saturate(strength * light.specific0 + light.specific1);
            if (angleAttenuation == 0.0)
                continue;

            attenuation *= sqr(angleAttenuation);
        }

        float NdotL = dot(-pointToLight / max(sqrt(sqrDistance), 0.001), rayDirection);
        float phaseFunction = ComputeScattering(uniforms.fog.scatteringAnisotropy, NdotL);
        float shadowValue = GetShadowFactorPunctualLight(j, lightType, shadowPosition);

        lightScattering += phaseFunction * shadowValue * light.color.rgb * attenuation * light.intensity * falloff;
    }

    return lightScattering;

}

vec3 ComputePunctualLightSample(uint lightType, ivec3 froxel, vec3 offset, vec2 volumetricResolution) {

    vec3 columnStart = normalize(GetFroxelPosition(froxel, vec3(0.0, 0.5, 1.0), volumetricResolution));
    vec3 columnEnd = normalize(GetFroxelPosition(froxel, vec3(1.0, 0.5, 1.0), volumetricResolution));
    float angularRadius = 0.5 * distance(columnStart, columnEnd);

    vec3 segmentStart = GetFroxelPosition(froxel, vec3(offset.xy, 0.0), volumetricResolution);
    vec3 segmentEnd = GetFroxelPosition(froxel, vec3(offset.xy, 1.0), volumetricResolution);

    float rayStart = length(segmentStart);
    float rayLength = length(segmentEnd);
    float rayEnd = min(rayLength, punctualLightDistance);

    return rayStart < rayEnd ? ComputePunctualLightScattering(lightType, segmentEnd / rayLength, rayStart, rayEnd, offset.z, angularRadius) : vec3(0.0);

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

vec3 GetSkyAmbient() {

#ifdef SKY_PROBE
    return 0.5 * (textureLod(diffuseProbe, vec3(0.0, 1.0, 0.0), 0.0).rgb + textureLod(diffuseProbe, vec3(0.0, -1.0, 0.0), 0.0).rgb);
#else
    return vec3(1.0);
#endif

}

#ifdef FOG_VOLUMES
float GetValueNoise(vec3 position) {

    ivec3 cell = ivec3(floor(position));
    vec3 weight = fract(position);
    weight = weight * weight * (3.0 - 2.0 * weight);

    float values[8];
    for (int i = 0; i < 8; i++) {
        uvec3 corner = uvec3(cell + ivec3(i & 1, (i >> 1) & 1, (i >> 2) & 1));
        // The hash is symmetric in its last components
        values[i] = floatConstruct(hash(corner + uvec3(0u, 0x3C6EF372u, 0x9E3779B9u)));
    }

    float bottom = mix(mix(values[0], values[1], weight.x), mix(values[2], values[3], weight.x), weight.y);
    float top = mix(mix(values[4], values[5], weight.x), mix(values[6], values[7], weight.x), weight.y);
    return mix(bottom, top, weight.z);

}

// Coverage is the part of the volume which keeps its fog, the rest is cut out as gaps
float GetFogVolumeNoise(FogVolume volume, vec3 worldPosition) {

    vec3 position = (worldPosition - volume.noiseOffsetAndFrequency.xyz) * volume.noiseOffsetAndFrequency.w;

    float noise = 0.0;
    float weight = 1.0;
    float totalWeight = 0.0;
    for (int i = 0; i < volume.noiseOctaves; i++) {
        noise += weight * GetValueNoise(position + 17.0 * float(i));
        totalWeight += weight;
        weight *= 0.5;
        position *= 2.03;
    }
    noise /= totalWeight;

    // Layered value noise rarely leaves this range
    float threshold = mix(0.8, 0.2, volume.noiseCoverage);
    float softness = mix(0.3, 0.01, volume.noiseSharpness);
    return smoothstep(threshold - softness, threshold + softness, noise);

}

float GetFogVolumeDensity(FogVolume volume, vec3 worldPosition) {

    vec3 unitPosition = GetFogVolumeUnitPosition(volume, worldPosition);

    vec3 absPosition = abs(unitPosition);
    float edgeDistance = 1.0 - (volume.shape == FOG_VOLUME_BOX ?
        max(absPosition.x, max(absPosition.y, absPosition.z)) : length(unitPosition));
    if (edgeDistance <= 0.0)
        return 0.0;

    float density = volume.scatteringAndDensity.a * smoothstep(0.0, max(volume.emissiveAndEdgeFade.a, 0.001), edgeDistance);
    density *= exp(-volume.heightFalloff * (unitPosition.y * 0.5 + 0.5));

    if (volume.noiseAmount > 0.0)
        density *= mix(1.0, GetFogVolumeNoise(volume, worldPosition), volume.noiseAmount);

    return density;

}

// Ignores the noise, the falloff and the fade at the edges
float GetFogVolumeTransmittanceToLight(FogVolume volume, vec3 worldPosition, vec3 worldToLight) {

    vec3 unitPosition = GetFogVolumeUnitPosition(volume, worldPosition);
    vec3 unitDirection = vec3(dot(volume.worldToUnit0.xyz, worldToLight), dot(volume.worldToUnit1.xyz, worldToLight),
        dot(volume.worldToUnit2.xyz, worldToLight));

    float exitDistance;
    if (volume.shape == FOG_VOLUME_BOX) {
        vec3 axisDistance = (vec3(1.0) - unitPosition * sign(unitDirection)) / max(abs(unitDirection), vec3(1e-6));
        exitDistance = min(axisDistance.x, min(axisDistance.y, axisDistance.z));
    }
    else {
        float a = dot(unitDirection, unitDirection);
        float b = dot(unitPosition, unitDirection);
        float c = dot(unitPosition, unitPosition) - 1.0;
        exitDistance = (-b + sqrt(max(b * b - a * c, 0.0))) / a;
    }

    // The second term stands in for light which reaches the point after scattering several times
    float opticalDepth = volume.selfShadowing * volume.scatteringAndDensity.a * max(exitDistance, 0.0);
    return max(exp(-opticalDepth), 0.3 * exp(-0.25 * opticalDepth));

}
#endif

void main() {

    // No early return, all invocations have to reach the barriers when loading the lights
    ivec3 froxel = ivec3(gl_GlobalInvocationID);
    bool validFroxel = all(lessThan(froxel.xy, pushConstants.columnCount)) && froxel.z < pushConstants.sliceCount;

    vec2 volumetricResolution = vec2(pushConstants.resolution);

    vec3 jitter = pushConstants.jitter.xyz;
    // The last slice reaches up to the camera far plane, a jittered depth makes the distance flicker
    if (froxel.z == pushConstants.sliceCount - 1)
        jitter.z = 0.5;
    vec3 position = GetFroxelPosition(froxel, jitter, volumetricResolution);
    vec3 rayDirection = normalize(position);
    vec3 worldPosition = vec3(globalData.ivMatrix * vec4(position, 1.0));

    float heightDensity = GetVolumetricFogDensity(uniforms.fog, worldPosition);
    float density = uniforms.fog.density * heightDensity;

    // Weights of the lighting in the volumes, known before the lighting is computed
    vec3 volumeDirectional = vec3(0.0);
    vec3 volumeLocal = vec3(0.0);
    vec3 volumeAmbient = vec3(0.0);
    vec3 volumeEmissive = vec3(0.0);
    float volumeExtinction = 0.0;

#ifdef FOG_VOLUMES
    vec3 worldToLight = -normalize(mat3(globalData.ivMatrix) * volumetricLights[0].direction.xyz);
    uint fogVolumeMask = fogVolumeMasks[GetFogVolumeClusterIndex()];
    while (fogVolumeMask != 0u) {
        FogVolume volume = fogVolumes[findLSB(fogVolumeMask)];
        fogVolumeMask &= fogVolumeMask - 1u;

        float volumeDensity = GetFogVolumeDensity(volume, worldPosition);
        if (volumeDensity <= 0.0)
            continue;

        float transmittance = uniforms.directionalLightCount > 0 ?
            GetFogVolumeTransmittanceToLight(volume, worldPosition, worldToLight) : 1.0;

        vec3 volumeScattering = volumeDensity * volume.scatteringAndDensity.rgb;
        volumeDirectional += transmittance * volumeScattering;
        volumeLocal += volumeScattering;
        volumeAmbient += volume.ambientFactor * volumeScattering;
        volumeEmissive += volumeDensity * volume.emissiveAndEdgeFade.rgb;
        volumeExtinction += volumeDensity;
    }
#endif

    // The lighting is multiplied by the density, shadows are expensive and don't matter without fog
    bool hasFog = validFroxel && ((density > 0.0 && heightDensity > 0.001) || volumeExtinction > 0.0);

    vec3 directionalScattering = vec3(0.0);
    for (int j = 0; hasFog && j < uniforms.directionalLightCount; j++) {
        VolumetricLight light = volumetricLights[j];

        uint lightType = floatBitsToUint(light.color.a);

        bool isMain = j == 0 ? true : false;
        float shadowValue = GetShadowFactorDirectionalLight(j, light.shadowIdx, lightType, isMain, position);

        vec3 lightPosition = position - 100000.0 * light.direction.xyz;
        lightPosition = vec3(globalData.ivMatrix * vec4(lightPosition, 1.0));
        float extinctionToLight = ComputeVolumetricFog(uniforms.fog, worldPosition, lightPosition);
        float NdotL = dot(rayDirection, normalize(light.direction.xyz));

        float phaseFunction = ComputeScattering(uniforms.fog.scatteringAnisotropy, NdotL);

        directionalScattering += shadowValue * phaseFunction * light.color.rgb * light.intensity * extinctionToLight;
    }

    vec3 center = GetFroxelPosition(froxel, vec3(0.5), volumetricResolution);

    vec3 localScattering = vec3(0.0);
#ifdef LOCAL_LIGHTS
    LoadLightTypeData(POINT_LIGHT);
    if (hasFog && sharedLightIndicesCount > 0)
        localScattering += ComputePunctualLightSample(POINT_LIGHT, froxel, pushConstants.jitter.xyz, volumetricResolution);
    LoadLightTypeData(SPOT_LIGHT);
    if (hasFog && sharedLightIndicesCount > 0)
        localScattering += ComputePunctualLightSample(SPOT_LIGHT, froxel, pushConstants.jitter.xyz, volumetricResolution);
#endif

    vec3 skyAmbient = hasFog ? GetSkyAmbient() : vec3(0.0);

    vec3 scattering = uniforms.fog.scatteringFactor * uniforms.fog.extinctionCoefficients.rgb * density *
        (directionalScattering + localScattering + uniforms.fog.ambientFactor * skyAmbient);
    scattering += volumeDirectional * directionalScattering + volumeLocal * localScattering +
        volumeAmbient * skyAmbient + volumeEmissive;
    float extinction = uniforms.fog.extinctionFactor * density + volumeExtinction;

    vec4 current = vec4(scattering, extinction) * FROXEL_DATA_SCALE;

    bool validHistory;
    vec4 history = GetHistory(center, volumetricResolution, validHistory);
    // A single invalid value would stay in the history forever
    validHistory = validHistory && !any(isnan(history)) && !any(isinf(history));
    vec4 result = validHistory ? mix(history, current, 0.1) : current;

    if (validFroxel)
        imageStore(froxelImage, froxel, result);

}
