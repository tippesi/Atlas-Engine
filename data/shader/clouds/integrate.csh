#include <../structures>
#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../common/random.hsh>

#include <cloudNoise.hsh>

layout (local_size_x = 8, local_size_y = 4) in;

layout(binding = 0) uniform sampler2D depthTexture;
layout(binding = 1) uniform sampler3D shapeTexture;
layout(binding = 0) writeonly uniform image2D volumetricCloudImage;

uniform Light light;
uniform float shapeScale = 1.0;
uniform int sampleCount = 128;
uniform mat4 vMatrix;
uniform mat4 ivMatrix;
uniform vec3 cameraLocation;

uniform float densityCutoff = 0.5;
uniform float densityMultiplier = 5.0;

uniform vec3 aabbMin = vec3(5.0);
uniform vec3 aabbMax = vec3(10.0);

vec3 ComputeVolumetricClouds(vec3 fragPos, vec2 texCoords);

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(volumetricCloudImage).x ||
        pixel.y > imageSize(volumetricCloudImage).y)
        return;

	vec2 texCoord = (vec2(pixel) + 0.5) / vec2(imageSize(volumetricCloudImage));

    float depth = textureLod(depthTexture, texCoord, 0.0).r;
    vec3 pixelPos = ConvertDepthToViewSpace(depth, texCoord);

    vec3 radiance = ComputeVolumetricClouds(pixelPos, texCoord);
    imageStore(volumetricCloudImage, pixel, vec4(radiance, 0.0));

}

const float ditherPattern[16] = float[](0.0, 0.5, 0.125, 0.625, 0.75, 0.22, 0.875, 0.375,
		0.1875, 0.6875, 0.0625, 0.5625, 0.9375, 0.4375, 0.8125, 0.3125);

bool IntersectAABB(vec3 direction, vec3 aabbMin, vec3 aabbMax, out float inDist, out float outDist) {

    vec3 inverseDirection = 1.0 / direction;

	vec3 t0 = aabbMin * inverseDirection;
	vec3 t1 = aabbMax * inverseDirection;
	
	vec3 tsmall = min(t0, t1);
	vec3 tbig = max(t0, t1);
	
	float tminf = max(max(0.0, tsmall.x), max(tsmall.y, tsmall.z));
	float tmaxf = min(min(1000.0, tbig.x), min(tbig.y, tbig.z));

	bool intersect = (tminf <= tmaxf);
	inDist = intersect ? tminf : 10000.0;
    outDist = intersect ? tmaxf : 10000.0;
	
	return intersect;

}

bool IntersectAABB(vec3 origin, vec3 direction, vec3 aabbMin, vec3 aabbMax, out float inDist, out float outDist) {

    vec3 inverseDirection = 1.0 / direction;

	vec3 t0 = (aabbMin - origin) * inverseDirection;
	vec3 t1 = (aabbMax - origin) * inverseDirection;
	
	vec3 tsmall = min(t0, t1);
	vec3 tbig = max(t0, t1);
	
	float tminf = max(max(0.0, tsmall.x), max(tsmall.y, tsmall.z));
	float tmaxf = min(min(1000.0, tbig.x), min(tbig.y, tbig.z));

	bool intersect = (tminf <= tmaxf);
	inDist = intersect ? tminf : 10000.0;
    outDist = intersect ? tmaxf : 10000.0;
	
	return intersect;

}

float SampleDensity(vec3 pos, vec3 weatherData) {

    vec4 lowFrequencyNoise = textureLod(shapeTexture, pos, 0.0);

    float lowFrequenyFBM = lowFrequencyNoise.g * 0.625
                         + lowFrequencyNoise.b * 0.250
                         + lowFrequencyNoise.a * 0.125;

    float baseCloudDensity = Remap(lowFrequencyNoise.r, 
        -(1.0 - lowFrequenyFBM), 1.0, 0.0, 1.0);

    const float densityHeightFalloff = 0.5;
    float densityHeightGradient = exp(densityHeightFalloff * pos.y);
    
    baseCloudDensity *= densityHeightGradient;
    return baseCloudDensity;

}


vec3 ComputeVolumetricClouds(vec3 fragPos, vec2 texCoords) {

    vec3 viewSpaceMin = vec3(vMatrix * vec4(aabbMin, 1.0));
    vec3 viewSpaceMax = vec3(vMatrix * vec4(aabbMax, 1.0));

    vec3 rayDirection = normalize(vec3(ivMatrix * vec4(fragPos, 0.0)));
    vec3 rayOrigin = cameraLocation;

    float inDist, outDist;
    /*
    if (!IntersectAABB(rayDirection, viewSpaceMin, viewSpaceMax, inDist, outDist))
        return vec3(0.0);
    float rayLength = length(fragPos);
    if (inDist > rayLength)
        return vec3(0.0);
    */
    if (!IntersectAABB(rayOrigin, rayDirection, aabbMin, aabbMax, inDist, outDist))
        return vec3(0.0);

    float rayLength = min(length(fragPos), outDist);
    if (inDist > rayLength)
        return vec3(0.0);

    float stepLength = (rayLength - inDist) / float(sampleCount);
    vec3 stepVector = rayDirection * stepLength;
 
    vec3 integration = vec3(0.0);
	vec3 rayPos = rayOrigin + rayDirection * inDist;

    vec3 aabbSize = aabbMin - aabbMax;

    for (int i = 0; i < sampleCount; i++) {
        
        vec3 shapeSamplePosition = (rayPos - aabbMin) / aabbSize;
        shapeSamplePosition.xz *= shapeScale;
        float densityAtPos = SampleDensity(shapeSamplePosition, vec3(1.0));

        rayPos += stepVector;

        integration += max(vec3(0.0), vec3(densityAtPos - densityCutoff) * densityMultiplier) * stepLength;

    }

    return vec3(1.0) - exp(-integration);

}