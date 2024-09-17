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

void main() {

    ivec2 resolution = textureSize(depthTexture, 0);
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(resolution);

    float depth = texelFetch(depthTexture, pixel, 0).r;
    float viewDepth = ConvertDepthToViewSpaceDepth(depth);

    if (gl_LocalInvocationIndex == 0u) {
        sharedDepthMin = floatBitsToUint(viewDepth);
        sharedDepthMax = floatBitsToUint(viewDepth);
    }

    int localOffset = int(gl_LocalInvocationIndex);
    for (int i = localOffset; i < lightBucketCount; i += groupSize) {
        sharedLightBuckets[i] = 0;
    }

    barrier();

    atomicMin(sharedDepthMin, floatBitsToUint(viewDepth));
    atomicMax(sharedDepthMax, floatBitsToUint(viewDepth));

    barrier();

    float depthMin = uintBitsToFloat(sharedDepthMin);
    float depthMax = uintBitsToFloat(sharedDepthMax);

    vec3 viewPos = ConvertDepthToViewSpace(depth, texCoord);
    
    for (int i = localOffset; i < pushConstants.lightCount; i += groupSize) {
        int lightBucketIdx = i / lightBucketCount;
        int lightBuckedBit = i % lightBucketCount;

        bool visible = true;

        Light light = lights[i];
        float radius = light.direction.w;

        uint lightType = floatBitsToUint(light.color.a);

        // Remember: Forward is in -z direction
        if (lightType == POINT_LIGHT) {
            /*
            vec3 pointToLight = light.location.xyz - viewPos;
            float sqrDistance = dot(pointToLight, pointToLight);
            float dist = sqrt(sqrDistance);
            lightMultiplier = saturate(1.0 - pow(dist / radius, 4.0)) / sqrDistance;

            surface.L = pointToLight / dist;
            */
            visible = depthMin > light.location.z - radius &&
                depthMax < light.location.z + radius;
        }
        else if (lightType == SPOT_LIGHT) {
            /*
            vec3 pointToLight = light.location.xyz - surface.P;
            float sqrDistance = dot(pointToLight, pointToLight);
            float dist = sqrt(sqrDistance);

            surface.L = pointToLight / dist;

            float strength = dot(surface.L, normalize(-light.direction.xyz));
            float attenuation = saturate(strength * light.specific0 + light.specific1);
            lightMultiplier = sqr(attenuation) / sqrDistance;
            */
            visible = depthMin > light.location.z - radius &&
                depthMax < light.location.z + radius;
        }

        

        if (visible) {
            atomicOr(sharedLightBuckets[lightBucketIdx], 1 << lightBuckedBit);
        }
    }

    barrier();

    int lightBucketOffset = GetLightBucketsGroupOffset();
    for (int i = localOffset; i < lightBucketCount; i += groupSize) {
        lightBuckets[i + lightBucketOffset] = sharedLightBuckets[i];
    }

}