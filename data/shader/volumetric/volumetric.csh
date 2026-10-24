#extension GL_EXT_nonuniform_qualifier : require

layout (local_size_x = 16, local_size_y = 16) in;

#include <volumetricLights.hsh>

#include <../common/ign.hsh>
#include <../common/stencil.hsh>
#include <../common/random.hsh>
#include <../common/bluenoise.hsh>

layout(set = 3, binding = 0, rgba16f) writeonly uniform image2D volumetricImage;
layout(set = 3, binding = 1) uniform sampler2D depthTexture;
layout(set = 3, binding = 3) uniform sampler2D oceanDepthTexture;
layout(set = 3, binding = 4) uniform usampler2D oceanStencilTexture;

vec4 ComputeVolumetricDirectionalLights(vec3 fragPos, float startDepth, vec2 texCoords);
vec4 ComputeVolumetricPunctualLights(uint lightType, vec3 fragPos, float startDepth, vec2 texCoords);

void main() {

    // No early return, all invocations have to reach the barriers when loading the lights
    ivec2 pixel = ivec2(gl_GlobalInvocationID);

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

            vec3 lightPosition = currentPosition - 100000.0 * light.direction.xyz;
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
            VolumetricLight light = sharedLights[j];

            float NdotL = 1.0;
            float attenuation = 1.0;
            float sqrDistance = 1.0;

            float shadowValue = 1.0;

            float radius = light.direction.w;
            if (lightType == POINT_LIGHT) {
                vec3 pointToLight = light.location.xyz - currentPosition;
                sqrDistance = max(0.0, dot(pointToLight, pointToLight));

                attenuation = saturate(1.0 - pow(sqrDistance / (radius * radius), 2.0));
                if (attenuation == 0.0)
                    continue;

                NdotL = dot(normalize(currentPosition - light.location.xyz), rayDirection);
                shadowValue = GetShadowFactorPunctualLight(j, POINT_LIGHT, currentPosition);
            }
            else if (lightType == SPOT_LIGHT) {
                vec3 pointToLight = light.location.xyz - currentPosition;
                sqrDistance = dot(pointToLight, pointToLight);

                vec3 L = pointToLight / sqrt(sqrDistance);

                float strength = dot(L, normalize(-light.direction.xyz));
                float angleAttenuation = saturate(strength * light.specific0 + light.specific1);
                float distAttenuation = saturate(1.0 - pow(sqrDistance / (radius * radius), 2.0));
                if (angleAttenuation == 0.0 || distAttenuation == 0.0)
                    continue;

                NdotL = dot(normalize(currentPosition - light.location.xyz), rayDirection);
                attenuation = min(distAttenuation * sqr(angleAttenuation), 1.0);
                shadowValue = GetShadowFactorPunctualLight(j, SPOT_LIGHT, currentPosition);
            }   

            // No shadows for now
            //shadowValue = 1.0;

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
