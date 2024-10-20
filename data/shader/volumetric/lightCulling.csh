layout (local_size_x = 16, local_size_y = 16) in;

#include <../deferred/lightCulling.hsh>

#include <../structures.hsh>
#include <../shadow.hsh>
#include <../globals.hsh>

#include <../common/convert.hsh>
#include <../common/utility.hsh>

layout(set = 3, binding = 1) uniform sampler2D depthTexture;

layout (std430, set = 3, binding = 8) buffer VolumetricLights {
    VolumetricLight volumetricLights[];
};

layout(std430, set = 3, binding = 10) buffer LightIndicesBuffer {
    int lightIndices[];
};

layout(push_constant) uniform constants {
    int lightCount;
} pushConstants;

const int lightCountPerTile = 63;
const int groupSize = int(gl_WorkGroupSize.x * gl_WorkGroupSize.y);

// Results in 32 * 128 = 4096 possible lights in the tile
shared int sharedLightIndices[lightCountPerTile];
shared int sharedLightCount;
shared uint sharedDepthMax;

int GetLightIndicesOffset() {

    int groupOffset = int(gl_WorkGroupID.x + gl_WorkGroupID.y * gl_NumWorkGroups.x);
    return groupOffset * (lightCountPerTile + 1);

}

void main() {

    ivec2 resolution = textureSize(depthTexture, 0);
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(resolution);

    float depth = texelFetch(depthTexture, pixel, 0).r;
    float viewDepth = ConvertDepthToViewSpaceDepth(depth);

    if (gl_LocalInvocationIndex == 0u) {
        sharedDepthMax = floatBitsToUint(depth);
        sharedLightCount = 0;
    }

    barrier();

    if (depth != 1.0) {
        atomicMax(sharedDepthMax, floatBitsToUint(depth));
    }

    barrier();

    float depthMin = 0.0;
    float depthMax = ConvertDepthToViewSpaceDepth(uintBitsToFloat(sharedDepthMax));

    vec3 frustumCorners[8];
    GetFrustumCorners(frustumCorners, resolution, 0.0, uintBitsToFloat(sharedDepthMax));

    Plane frustumPlanes[4];
    GetFrustumPlanes(frustumPlanes, frustumCorners);

    AABB frustumAABB = CalculateAABB(frustumCorners);

    vec3 viewPos = ConvertDepthToViewSpace(depth, texCoord);
    
    int localOffset = int(gl_LocalInvocationIndex);
    int lightIdx = 0;
    for (int i = localOffset; i < pushConstants.lightCount && lightIdx < lightCountPerTile; i += groupSize) {
        bool visible = true;

        VolumetricLight light = volumetricLights[i];
        float radius = light.direction.w;

        uint lightType = floatBitsToUint(light.color.a);
        Sphere sphere;

        // Remember: Forward is in -z direction
        if (lightType == POINT_LIGHT) {            
            sphere.center = light.location.xyz;
            sphere.radius = radius;

            visible = SphereAABBIntersection(sphere, frustumAABB);    
        }
        else if (lightType == SPOT_LIGHT) {
            sphere = CalculateSphereFromSpotLight(light.location, light.direction, radius);
        }

        visible = visible && SphereAABBIntersection(sphere, frustumAABB);
        if (visible) {
            visible = SphereFrustumIntersection(sphere, frustumPlanes);
        }

        if (visible) {
            lightIdx = atomicAdd(sharedLightCount, 1);

            if (lightIdx < lightCountPerTile)
                sharedLightIndices[lightIdx] =  i;
        }
    }

    barrier();

    int lightCount = min(lightCountPerTile, sharedLightCount);
    int lightIndicesOffset = GetLightIndicesOffset();
    for (int i = localOffset; i < lightCount; i += groupSize) {
        lightIndices[i + lightIndicesOffset + 1] = sharedLightIndices[i];
    }

    if (gl_LocalInvocationIndex == 0u) {
        lightIndices[lightIndicesOffset] = lightCount;
    }

}