#include <deferred.hsh>
#include <lightCulling.hsh>

#include <../structures.hsh>
#include <../shadow.hsh>
#include <../globals.hsh>

#include <../common/convert.hsh>
#include <../common/utility.hsh>

layout (local_size_x = 16, local_size_y = 16) in;

layout(push_constant) uniform constants {
    int lightCount;
} pushConstants;

const int groupSize = int(gl_WorkGroupSize.x * gl_WorkGroupSize.y);

// Results in 32 * 128 = 4096 possible lights in the tile
shared int sharedLightBuckets[lightBucketCount];
shared uint sharedDepthMin;
shared uint sharedDepthMax;
shared int depthMask;

struct AABB {
    vec3 center;
    vec3 extent;
};

struct Sphere {
    vec3 center;
    float radius;
};

void GetFrustumCorners(out vec3 frustumCorners[8]) {

    ivec2 resolution = textureSize(depthTexture, 0);

    ivec2 basePixel = ivec2(gl_WorkGroupID) * ivec2(gl_WorkGroupSize);
    vec2 baseTexCoord = vec2(basePixel) / vec2(resolution);
    vec2 texCoordSpan = vec2(basePixel + ivec2(gl_WorkGroupSize)) / vec2(resolution) - baseTexCoord;

    float depthMin = uintBitsToFloat(sharedDepthMin);
    float depthMax = uintBitsToFloat(sharedDepthMax);

    frustumCorners[0] = ConvertDepthToViewSpace(depthMin, baseTexCoord + vec2(0.0, 0.0));
    frustumCorners[1] = ConvertDepthToViewSpace(depthMin, baseTexCoord + vec2(texCoordSpan.x, 0.0));
    frustumCorners[2] = ConvertDepthToViewSpace(depthMin, baseTexCoord + vec2(0.0, texCoordSpan.y));
    frustumCorners[3] = ConvertDepthToViewSpace(depthMin, baseTexCoord + vec2(texCoordSpan.x, texCoordSpan.y));
    frustumCorners[4] = ConvertDepthToViewSpace(depthMax, baseTexCoord + vec2(0.0, 0.0));
    frustumCorners[5] = ConvertDepthToViewSpace(depthMax, baseTexCoord + vec2(texCoordSpan.x, 0.0));
    frustumCorners[6] = ConvertDepthToViewSpace(depthMax, baseTexCoord + vec2(0.0, texCoordSpan.y));
    frustumCorners[7] = ConvertDepthToViewSpace(depthMax, baseTexCoord + vec2(texCoordSpan.x, texCoordSpan.y));

}

AABB CalculateAABB(vec3 corners[8]) {

    vec3 aabbMin = corners[0];
    vec3 aabbMax = corners[0];

    for (int i = 1; i < 8; i++) {
        aabbMin = min(corners[i], aabbMin);
        aabbMax = max(corners[i], aabbMax);
    }

    AABB aabb;
    aabb.center = (aabbMax + aabbMin) * 0.5;
    aabb.extent = aabbMax - aabb.center;
    return aabb;

}

bool SphereAABBIntersection(Sphere sphere, AABB aabb) {

    vec3 diff = max(vec3(0.0), abs(aabb.center - sphere.center) - aabb.extent);
    return dot(diff, diff) <= sphere.radius * sphere.radius;

}

void main() {

    ivec2 resolution = textureSize(depthTexture, 0);
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(resolution);

    float depth = texelFetch(depthTexture, pixel, 0).r;
    float viewDepth = ConvertDepthToViewSpaceDepth(depth);

    if (gl_LocalInvocationIndex == 0u) {
        sharedDepthMin = floatBitsToUint(depth);
        sharedDepthMax = floatBitsToUint(depth);
        depthMask = 0;
    }

    int localOffset = int(gl_LocalInvocationIndex);
    for (int i = localOffset; i < lightBucketCount; i += groupSize) {
        sharedLightBuckets[i] = 0;
    }

    barrier();

    atomicMin(sharedDepthMin, floatBitsToUint(depth));
    atomicMax(sharedDepthMax, floatBitsToUint(depth));

    barrier();

    float depthMin = ConvertDepthToViewSpaceDepth(uintBitsToFloat(sharedDepthMin));
    float depthMax = ConvertDepthToViewSpaceDepth(uintBitsToFloat(sharedDepthMax));

    float depthRange = depthMax - depthMin;
    float invBinSize = 32.0 / depthRange;
    int depthBin = clamp(int((viewDepth - depthMin) * invBinSize), 0, 31);
    atomicOr(depthMask, 1 << depthBin);

    barrier();

    vec3 frustumCorners[8];
    GetFrustumCorners(frustumCorners);

    AABB frustumAABB = CalculateAABB(frustumCorners);

    vec3 viewPos = ConvertDepthToViewSpace(depth, texCoord);
    
    for (int i = localOffset; i < pushConstants.lightCount; i += groupSize) {
        int lightBucketIdx = i / 32;
        int lightBuckedBit = i % 32;

        bool visible = true;

        Light light = lights[i];
        float radius = light.direction.w;

        uint lightType = floatBitsToUint(light.color.a);

        int startBin = 0;
        int endBin = 31;

        // Remember: Forward is in -z direction
        if (lightType == POINT_LIGHT) {
            Sphere sphere;
            sphere.center = light.location.xyz;
            sphere.radius = radius;

            visible = SphereAABBIntersection(sphere, frustumAABB);
            
            startBin = clamp(int((light.location.z + radius - depthMin) * invBinSize), 0, 31);
            endBin = clamp(int((light.location.z - radius - depthMin) * invBinSize), 0, 31);            
        }
        else if (lightType == SPOT_LIGHT) {
            Sphere sphere;
            sphere.center = light.location.xyz;
            sphere.radius = radius;

            visible = SphereAABBIntersection(sphere, frustumAABB);

            startBin = clamp(int((light.location.z + radius - depthMin) * invBinSize), 0, 31);
            endBin = clamp(int((light.location.z - radius - depthMin) * invBinSize), 0, 31);
        }

        int shiftCount = 31 - endBin + startBin;
        int downShift = (0xFFFFFFFF >> shiftCount);
        int lightMask = downShift << startBin;

        if (visible && (lightMask & depthMask) != 0) {
            atomicOr(sharedLightBuckets[lightBucketIdx], 1 << lightBuckedBit);
        }
    }

    barrier();

    int lightBucketOffset = GetLightBucketsGroupOffset();
    for (int i = localOffset; i < lightBucketCount; i += groupSize) {
        lightBuckets[i + lightBucketOffset] = sharedLightBuckets[i];
    }

}