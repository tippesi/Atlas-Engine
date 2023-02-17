#include <../globals.hsh>
#include <../structures>
#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../common/random.hsh>
#include <../common/flatten.hsh>
#include <../common/bluenoise.hsh>

#include <clouds.hsh>

layout (local_size_x = 8, local_size_y = 4) in;

layout(set = 3, binding = 0, rgba16f) writeonly uniform image2D volumetricCloudImage;
layout(set = 3, binding = 1) uniform sampler2D depthTexture;
layout(set = 3, binding = 2) uniform sampler3D shapeTexture;
layout(set = 3, binding = 3) uniform sampler3D detailTexture;

layout(set = 3, binding = 4) uniform sampler2D scramblingRankingTexture;
layout(set = 3, binding = 5) uniform sampler2D sobolSequenceTexture;

layout(std140, set = 3, binding = 6) uniform UniformBuffer {
    Light light;

    float planetRadius;
    float innerRadius;
    float outerRadius;
    float distanceLimit;

    float lowerHeightFalloff;
    float upperHeightFalloff;

    float shapeScale;
    float detailScale;
    float shapeSpeed;
    float detailSpeed;
    float detailStrength;

    float extinctionFactor;
    float scatteringFactor;

    float eccentricityFirstPhase;
    float eccentricitySecondPhase;
    float phaseAlpha;

    float densityMultiplier;

    float time;
    uint frameSeed1;
    uint frameSeed2;
} uniforms;

const float epsilon = 0.001;
const int sampleCount = 32;
const vec3 windDirection = normalize(vec3(0.1, -0.4, 0.1));
const float windSpeed = 0.01;

const vec3 coefficient = vec3(0.71 * 0.05, 0.86 * 0.05, 1.0 * 0.05);

vec3 planetCenter = -vec3(0.0, uniforms.planetRadius, 0.0);
vec4 blueNoiseVec = vec4(0.0);

vec4 ComputeVolumetricClouds(vec3 fragPos, float depth, vec2 texCoords);

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(volumetricCloudImage).x ||
        pixel.y > imageSize(volumetricCloudImage).y)
        return;

	vec2 texCoord = (vec2(pixel) + 0.5) / vec2(imageSize(volumetricCloudImage));

    float depth = textureLod(depthTexture, texCoord, 0.0).r;
    vec3 pixelPos = ConvertDepthToViewSpace(depth, texCoord);

    int sampleIdx = int(uniforms.frameSeed1);
	blueNoiseVec = vec4(
			SampleBlueNoise(pixel, sampleIdx, 0, scramblingRankingTexture, sobolSequenceTexture),
			SampleBlueNoise(pixel, sampleIdx, 1, scramblingRankingTexture, sobolSequenceTexture),
            SampleBlueNoise(pixel, sampleIdx, 2, scramblingRankingTexture, sobolSequenceTexture),
            SampleBlueNoise(pixel, sampleIdx, 3, scramblingRankingTexture, sobolSequenceTexture)
			);

    vec4 scattering = ComputeVolumetricClouds(pixelPos, depth, texCoord);
    imageStore(volumetricCloudImage, pixel, scattering);

}

const float ditherPattern[16] = float[](0.0, 0.5, 0.125, 0.625, 0.75, 0.22, 0.875, 0.375,
		0.1875, 0.6875, 0.0625, 0.5625, 0.9375, 0.4375, 0.8125, 0.3125);

float GetDitherOffset(int idx) {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    int ditherIdx = ((int(pixel.x) % 4) * 4 + int(pixel.y) % 4 + idx) % 16;
    return ditherPattern[ditherIdx];

}

float GetNoiseOffset(int idx) {

   return blueNoiseVec[idx % 4];

}

vec2 IntersectSphere(vec3 origin, vec3 direction, vec3 pos, float radius) {

	vec3 L = pos - origin;
	float DT = dot(L, direction);
	float r2 = radius * radius;
	
	float ct2 = dot(L, L) - DT * DT;
	
	if (ct2 > r2)
		return vec2(-1.0);
	
	float AT = sqrt(r2 - ct2);
	float BT = AT;
	
	float AO = DT - AT;
	float BO = DT + BT;

    float minDist = min(AO, BO);
    float maxDist = max(AO, BO);

    return vec2(minDist, maxDist);
}

float SampleDensity(vec3 pos, vec3 shapeTexCoords, vec3 detailTexCoords,
     vec3 weatherData, float lod) {

    float baseCloudDensity = textureLod(shapeTexture, shapeTexCoords, lod).r;

    float heightFraction = shapeTexCoords.y;
    float densityHeightGradient = exp(-uniforms.upperHeightFalloff * heightFraction) * 
        exp(-(1.0 - heightFraction) * uniforms.lowerHeightFalloff);    
    
    baseCloudDensity *= densityHeightGradient;

    const float cloudCoverage = uniforms.densityMultiplier;
    baseCloudDensity = Remap(baseCloudDensity, cloudCoverage,
        1.0, 0.0, 1.0);
    float finalCloudDensity = baseCloudDensity;

    if (baseCloudDensity > 0.0) {
        float highFrequencyFBM = textureLod(detailTexture, detailTexCoords, lod).r;

        float highFrequencyNoiseModifier = mix(highFrequencyFBM,
            1.0 - highFrequencyFBM, saturate(heightFraction * 10.0));

        finalCloudDensity = Remap(baseCloudDensity, 
            highFrequencyNoiseModifier * uniforms.detailStrength, 1.0, 0.0, 1.0);
    }

    return saturate(finalCloudDensity);

}

void CalculateTexCoords(vec3 pos, out vec3 shapeTexCoords, out vec3 detailTexCoords) {

    float distFromCenter = distance(pos, planetCenter);

    shapeTexCoords.xz = pos.xz * uniforms.shapeScale * 0.001;
    shapeTexCoords.y = (distFromCenter - uniforms.innerRadius) / (uniforms.outerRadius - uniforms.innerRadius);
    shapeTexCoords.xz += windDirection.xz * uniforms.shapeSpeed * windSpeed * uniforms.time;

    detailTexCoords.xz = pos.xz * uniforms.shapeScale * 0.001;
    detailTexCoords.y = (distFromCenter - uniforms.innerRadius) / (uniforms.outerRadius - uniforms.innerRadius);
    detailTexCoords *= uniforms.detailScale;
    detailTexCoords += windDirection * uniforms.detailSpeed * windSpeed * uniforms.time;

}

void CalculateRayLength(vec3 rayOrigin, vec3 rayDirection, out float minDist, out float maxDist) {

    vec3 spherePos = planetCenter;
    vec2 planetDist = IntersectSphere(rayOrigin, rayDirection, spherePos, uniforms.planetRadius);
    vec2 inDist = IntersectSphere(rayOrigin, rayDirection, spherePos, uniforms.innerRadius);
    vec2 outDist = IntersectSphere(rayOrigin, rayDirection, spherePos, uniforms.outerRadius);

    // We're in the inner layer
    if (inDist.x < 0.0 && inDist.y >= 0.0) {
        // When the planet is in front of the inner layer set dist to zero
        minDist = planetDist.x >= 0.0 ? 0.0 : inDist.y;
        maxDist = planetDist.x >= 0.0 ? 0.0 : outDist.y;
    }
    else {
        // We can't see the inner layer
        if (inDist.x < 0.0 && inDist.y < 0.0) {
            minDist = 0.0;
            // We might see the outer layer or not
            maxDist = max(0.0, outDist.y);
        }
        // We can see the inner layer and are in between layers
        if (inDist.x > 0.0 && inDist.y > 0.0 &&
            outDist.x < 0.0 && outDist.y >= 0.0) {
            minDist = 0.0;
            maxDist = planetDist.x >= 0.0 ? inDist.x : outDist.y;
        }        
        // We are out of both layers and looking down
        if (outDist.x > 0.0 && outDist.y > 0.0) {
            minDist = outDist.x;
            maxDist = inDist.x >= 0.0 ? inDist.x : outDist.x;
        }
    }

    // If the distance gets to big, we might undersample near detail which can go missing
    // This is especially the case when we are in between the two radii layers
    minDist = min(uniforms.distanceLimit, minDist);
    maxDist = min(uniforms.distanceLimit, maxDist);

}

float GetExtinctionToLight(vec3 pos, int ditherIdx) {

    const int lightSampleCount = 5;

    vec3 rayDirection = -normalize(uniforms.light.direction.xyz);

    float inDist, outDist;
    CalculateRayLength(pos, rayDirection, inDist, outDist);

    float rayLength = 0.25 * (outDist - inDist);
    float stepLength = rayLength / float(lightSampleCount);
    vec3 stepVector = rayDirection * stepLength;

    // Dither secondary rays
    pos += stepVector * GetNoiseOffset(ditherIdx);

    float lod = 0.5;

    float extinctionAccumulation = 0.0;
    for (int i = 0; i < lightSampleCount; i++) {
        /*
        if (extinction <= epsilon) {
            extinction = 0.0;
            break;
        }
        */

        vec3 shapeTexCoords, detailTexCoords;
        CalculateTexCoords(pos, shapeTexCoords, detailTexCoords);

        float density = saturate(SampleDensity(pos, shapeTexCoords, detailTexCoords, vec3(1.0), floor(0.0)));
        float extinctionCoefficient = uniforms.extinctionFactor * density;

        extinctionAccumulation += extinctionCoefficient * stepLength;
        
        pos += stepVector;
        // lod += 0.5;
    }

    return exp(-extinctionAccumulation);

}

vec3 ComputeAmbientColor(vec3 pos, float extinctionCoefficient) {

    float distFromCenter = distance(pos, planetCenter);

    vec3 isotropicLightTop = vec3(0.3, 0.4, 0.6);
    float heightFraction = (distFromCenter - uniforms.innerRadius) / (uniforms.outerRadius - uniforms.innerRadius);
    float ambientContribution = saturate(0.1);
    return isotropicLightTop * ambientContribution;

}

float DualPhaseFunction(float g0, float g1, float alpha, float LDotV) {

    return mix(ComputeScattering(LDotV, g0), ComputeScattering(LDotV, g1), alpha);

}

vec4 ComputeVolumetricClouds(vec3 fragPos, float depth, vec2 texCoords) {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);

    vec3 rayDirection = normalize(vec3(globalData.ivMatrix * vec4(fragPos, 0.0)));
    vec3 rayOrigin = globalData.cameraLocation.xyz;

    float inDist, outDist;
    CalculateRayLength(rayOrigin, rayDirection, inDist, outDist);

    if (inDist <= 0.0 && outDist <= 0.0)
        return vec4(0.0, 0.0, 0.0, 1.0);

    float rayLength = depth < 1.0 ? min(length(fragPos), outDist - inDist) : outDist - inDist;
    if (inDist > rayLength && depth < 1.0)
        return vec4(0.0, 0.0, 0.0, 1.0);

    int raySampleCount = max(sampleCount, int((rayLength / uniforms.distanceLimit) * float(sampleCount)));
    float stepLength = rayLength / float(raySampleCount);
    vec3 stepVector = rayDirection * stepLength;

    // Primary noise always uses index 0
    float noiseValue = GetNoiseOffset(int(0));
	rayOrigin += stepVector * noiseValue;
 
    vec3 integration = vec3(0.0);
	vec3 rayPos = rayOrigin + rayDirection * inDist;

    float extinction = 1.0;
    vec3 scattering = vec3(0.0);

    // Secondary noise uses frame seed, seems to help banding
    int noiseIdx = int(uniforms.frameSeed2);
    for (int i = 0; i < raySampleCount; i++) {

        if (extinction <= epsilon) {
            break;
        }
        
        vec3 shapeTexCoords, detailTexCoords;
        CalculateTexCoords(rayPos, shapeTexCoords, detailTexCoords);

        float distToPlanetCenter = distance(rayPos, planetCenter);
        // This can happen if we look from inside the cloud layer trough it below the cloud
        // layer. We need to find the next intersection with the inner layer
        if (distToPlanetCenter < uniforms.innerRadius) {
            /*
            CalculateRayLength(rayPos, rayDirection, inDist, outDist);
            stepLength = rayLength / float(sampleCount - i);
            stepVector = rayDirection * stepLength;
            rayPos += rayDirection * inDist;
            */
            rayPos += stepVector;
            continue;
        }

        float density = saturate(SampleDensity(rayPos, shapeTexCoords, detailTexCoords, vec3(1.0), 0.0));

        if (density > 0.0) {
            float scatteringCoefficient = uniforms.scatteringFactor * density;
            float extinctionCoefficient = uniforms.extinctionFactor * density;

            float clampedExtinction = max(extinctionCoefficient, 0.0000001);
            float stepExtinction = exp(-extinctionCoefficient * stepLength);

            float lightDotView = dot(normalize(uniforms.light.direction.xyz), normalize(rayDirection));
            vec3 lightColor = uniforms.light.color.rgb * uniforms.light.intensity;
            float lightExtinction =  GetExtinctionToLight(rayPos, noiseIdx++);

            float phaseFunction = DualPhaseFunction(uniforms.eccentricityFirstPhase, 
                uniforms.eccentricitySecondPhase, uniforms.phaseAlpha, lightDotView);
            vec3 stepScattering = scatteringCoefficient * (phaseFunction * lightColor * lightExtinction
                + ComputeAmbientColor(rayPos, extinctionCoefficient));

            vec3 luminanceIntegral = (stepScattering - stepScattering * stepExtinction) / clampedExtinction;
            scattering += luminanceIntegral * extinction;
            extinction *= stepExtinction;
        }

        rayPos += stepVector;
    }

    return vec4(scattering, extinction);

}