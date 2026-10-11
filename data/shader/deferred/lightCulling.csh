layout (local_size_x = 16, local_size_y = 16) in;

#include <deferred.hsh>
#include <lightCulling.hsh>

#include <../structures.hsh>
#include <../shadow.hsh>
#include <../globals.hsh>

#include <../common/convert.hsh>
#include <../common/utility.hsh>

layout(push_constant) uniform constants {
    int lightCount;
} pushConstants;

layout(std430, set = 3, binding = 6) buffer LightBucketsBuffer {
    int lightBuckets[];
};

const int groupSize = int(gl_WorkGroupSize.x * gl_WorkGroupSize.y);

// Results in 32 * 128 = 4096 possible lights in the tile
shared int sharedLightBuckets[lightBucketCount];
shared uint sharedDepthMin;
shared uint sharedDepthMax;
shared int depthMask;

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

    if (depth != 1.0) {
        atomicMin(sharedDepthMin, floatBitsToUint(depth));
        atomicMax(sharedDepthMax, floatBitsToUint(depth));
    }

    barrier();

    float depthMin = ConvertDepthToViewSpaceDepth(uintBitsToFloat(sharedDepthMin));
    float depthMax = ConvertDepthToViewSpaceDepth(uintBitsToFloat(sharedDepthMax));

    float depthRange = depthMax - depthMin;
    float invBinSize = 32.0 / depthRange;
    int depthBin = clamp(int((viewDepth - depthMin) * invBinSize), 0, 31);
    atomicOr(depthMask, 1 << depthBin);

    barrier();

    vec3 frustumCorners[8];
    GetFrustumCorners(frustumCorners, resolution, uintBitsToFloat(sharedDepthMin), uintBitsToFloat(sharedDepthMax));

    Plane frustumPlanes[4];
    GetFrustumPlanes(frustumPlanes, frustumCorners);

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

        Sphere sphere;

        // Remember: Forward is in -z direction
        if (lightType == POINT_LIGHT) {
            sphere.center = light.location.xyz;
            sphere.radius = radius;
            
            startBin = clamp(int((light.location.z + radius - depthMin) * invBinSize), 0, 31);
            endBin = clamp(int((light.location.z - radius - depthMin) * invBinSize), 0, 31);            
        }
        else if (lightType == SPOT_LIGHT) {
            sphere = CalculateSphereFromSpotLight(light.location, light.direction, radius);

            startBin = clamp(int((light.location.z + radius - depthMin) * invBinSize), 0, 31);
            endBin = clamp(int((light.location.z - radius - depthMin) * invBinSize), 0, 31);
        }

        if (lightType != DIRECTIONAL_LIGHT) {
            visible = visible && SphereAABBIntersection(sphere, frustumAABB);
            if (visible) {
                visible = SphereFrustumIntersection(sphere, frustumPlanes);
            }
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