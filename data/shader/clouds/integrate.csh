#include <../globals.hsh>
#include <../structures>
#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../common/random.hsh>
#include <../common/flatten.hsh>

#include <clouds.hsh>

layout (local_size_x = 8, local_size_y = 4) in;

layout(set = 3, binding = 0, rgba16f) writeonly uniform image2D volumetricCloudImage;
layout(set = 3, binding = 1) uniform sampler2D depthTexture;
layout(set = 3, binding = 2) uniform sampler3D shapeTexture;
layout(set = 3, binding = 3) uniform sampler3D detailTexture;
layout(set = 3, binding = 4) uniform sampler2D randomTexture;

layout(std140, set = 3, binding = 5) uniform UniformBuffer {
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

    float eccentricity;
    float extinctionFactor;
    float scatteringFactor;

    float silverLiningSpread;
    float silverLiningIntensity;

    float densityMultiplier;

    float time;
    uint frameSeed;
} uniforms;

const int sampleCount = 128;
const vec3 windDirection = normalize(vec3(0.1, -0.4, 0.1));
const float windSpeed = 0.01;

vec3 planetCenter = -vec3(0.0, uniforms.planetRadius, 0.0);

vec4 ComputeVolumetricClouds(vec3 fragPos, float depth, vec2 texCoords);

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(volumetricCloudImage).x ||
        pixel.y > imageSize(volumetricCloudImage).y)
        return;

	vec2 texCoord = (vec2(pixel) + 0.5) / vec2(imageSize(volumetricCloudImage));

    float depth = textureLod(depthTexture, texCoord, 0.0).r;
    vec3 pixelPos = ConvertDepthToViewSpace(depth, texCoord);

    vec4 scattering = ComputeVolumetricClouds(pixelPos, depth, texCoord);
    imageStore(volumetricCloudImage, pixel, scattering);

}

const float ditherPattern[16] = float[](0.0, 0.5, 0.125, 0.625, 0.75, 0.22, 0.875, 0.375,
		0.1875, 0.6875, 0.0625, 0.5625, 0.9375, 0.4375, 0.8125, 0.3125);

float GetDitherOffset(int idx) {

    return ditherPattern[idx];

}

float GetNoiseOffset(int idx) {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);

    ivec2 noiseOffset = Unflatten2D(idx % 256, ivec2(16)) * ivec2(16);
    float blueNoise = texelFetch(randomTexture, (pixel + noiseOffset) % ivec2(128), 0).r * 256.0;
	blueNoise = clamp(blueNoise, 0.0, 255.0);
	blueNoise = (blueNoise + 0.5) / 256.0;
    return blueNoise;

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
     vec3 weatherData) {

    float lod = 0.0;

    vec2 lowFrequencyNoise = textureLod(shapeTexture, shapeTexCoords, lod).rg;

    float lowFrequencyBaseNoise = lowFrequencyNoise.r;
    float lowFrequencyFBM = lowFrequencyNoise.g;

    float baseCloudDensity = Remap(lowFrequencyBaseNoise,
        -(1.0 - lowFrequencyFBM), 1.0, 0.0, 1.0);

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

    const int lightSampleCount = 8;

    vec3 rayDirection = -normalize(uniforms.light.direction.xyz);

    float inDist, outDist;
    CalculateRayLength(pos, rayDirection, inDist, outDist);

    float stepLength = 0.25 * (outDist - inDist)  / float(lightSampleCount);
    vec3 stepVector = rayDirection * stepLength;

    // Dither secondary rays
    pos += stepVector * GetDitherOffset(ditherIdx);

    float extinction = 1.0;
    for (int i = 0; i < lightSampleCount; i++) {        
        vec3 shapeTexCoords, detailTexCoords;
        CalculateTexCoords(pos, shapeTexCoords, detailTexCoords);

        float density = saturate(SampleDensity(pos, shapeTexCoords, detailTexCoords, vec3(1.0)));
        float extinctionCoefficient = uniforms.extinctionFactor * density;

        extinction *= exp(-extinctionCoefficient * stepLength);
        
        pos += stepVector;
    }

    return extinction;

}

// Based on http://patapom.com/topics/Revision2013/Revision%202013%20-%20Real-time%20Volumetric%20Rendering%20Course%20Notes.pdf
float Ei(float z) {

    return 0.5772156649015328606065 + log(1e-4 + abs(z)) + z * (1.0 + z * (0.25 + z * ((1.0/18.0)
         + z * ((1.0/96.0) + z * (1.0/600.0))))); // For x!=0

}

vec3 ComputeAmbientColor(vec3 pos, float extinctionCoefficient) {

    float distFromCenter = distance(pos, planetCenter);

    vec3 isotropicLightTop = vec3(0.2);
    vec3 isotropicLightBottom = vec3(0.2);
    float Hp = uniforms.outerRadius - distFromCenter; // Height to the top of the volume
    float a = -extinctionCoefficient * Hp;
    vec3 isotropicScatteringTop = isotropicLightTop * max(0.0, exp(a) - a * Ei(a));
    float Hb = distFromCenter - uniforms.innerRadius; // Height to the bottom of the volume
    a = -extinctionCoefficient * Hb;
    vec3 isotropicScatteringBottom = isotropicLightBottom * max(0.0, exp(a) - a * Ei(a));
    return isotropicScatteringTop + isotropicScatteringBottom;

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

    int raySampleCount = max(128, int((rayLength / uniforms.distanceLimit) * float(sampleCount)));
    float stepLength = rayLength / float(raySampleCount);
    vec3 stepVector = rayDirection * stepLength;

    int ditherIdx = ((int(pixel.x) % 4) * 4 + int(pixel.y) % 4 + int(uniforms.frameSeed)) % 16;
    float ditherValue = GetDitherOffset(ditherIdx);
	rayOrigin += stepVector * ditherValue;
 
    vec3 integration = vec3(0.0);
	vec3 rayPos = rayOrigin + rayDirection * inDist;

    float extinction = 1.0;
    vec3 scattering = vec3(0.0);

    for (int i = 0; i < raySampleCount; i++) {
        
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

        float density = saturate(SampleDensity(rayPos, shapeTexCoords, detailTexCoords, vec3(1.0)));

        if (density > 0.0) {
            float scatteringCoefficient = uniforms.scatteringFactor * density;
            float extinctionCoefficient = uniforms.extinctionFactor * density;

            float stepExtinction = exp(-extinctionCoefficient * stepLength);
            extinction *= stepExtinction;

            float lightDotView = dot(normalize(uniforms.light.direction.xyz), normalize(rayDirection));
            vec3 lightColor = uniforms.light.color.rgb * uniforms.light.intensity;
            float lightExtinction =  GetExtinctionToLight(rayPos, ditherIdx);

            float standardLightPhase = ComputeScattering(lightDotView, uniforms.eccentricity);
            float silverLiningPhase = uniforms.silverLiningIntensity * ComputeScattering(lightDotView, -1.0 + uniforms.silverLiningSpread);
            float lightPhase = max(standardLightPhase, silverLiningPhase);
            vec3 stepScattering = scatteringCoefficient  * stepLength * (lightPhase * lightColor * lightExtinction
                + ComputeAmbientColor(rayPos, extinctionCoefficient));

            vec3 luminanceIntegral = stepScattering * stepExtinction;
            scattering += luminanceIntegral * extinction;
        }

        rayPos += stepVector;
    }

    // Workaround for now
    vec3 ambient = vec3(0.2) * (1.0 - dot(scattering, vec3(0.333))) * (1.0 - extinction);
    return vec4(scattering + ambient, extinction);

}