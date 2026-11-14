// One light list per froxel cluster

layout (local_size_x = 64) in;

#include <../deferred/lightCulling.hsh>
#include <../structures.hsh>
#include <../globals.hsh>
#include <../common/convert.hsh>

#include <froxel.hsh>
#include <fogVolumes.hsh>

layout (std430, set = 3, binding = 8) buffer VolumetricLights {
    VolumetricLight volumetricLights[];
};

layout(std430, set = 3, binding = 10) buffer LightIndicesBuffer {
    int lightIndices[];
};

const int lightCountPerCluster = 63;

shared int sharedLightIndices[lightCountPerCluster];
shared int sharedLightCount;
#ifdef FOG_VOLUMES
shared uint sharedFogVolumeMask;
#endif

void main() {

    ivec3 cluster = ivec3(gl_WorkGroupID);
    ivec3 firstFroxel = cluster * FROXEL_CLUSTER_SIZE;
    ivec3 lastFroxel = min(firstFroxel + FROXEL_CLUSTER_SIZE, ivec3(pushConstants.columnCount, pushConstants.sliceCount));

    vec2 volumetricResolution = vec2(pushConstants.resolution);

    vec3 corners[8];
    for (int i = 0; i < 8; i++) {
        ivec3 froxel = ivec3((i & 1) > 0 ? lastFroxel.x : firstFroxel.x, (i & 2) > 0 ? lastFroxel.y : firstFroxel.y,
            (i & 4) > 0 ? lastFroxel.z - 1 : firstFroxel.z);
        vec3 offset = vec3(0.0, 0.0, (i & 4) > 0 ? 1.0 : 0.0);
        corners[i] = GetFroxelPosition(froxel, offset, volumetricResolution);
    }

    Plane planes[4];
    GetFrustumPlanes(planes, corners);
    AABB aabb = CalculateAABB(corners);

    if (gl_LocalInvocationIndex == 0u) {
        sharedLightCount = 0;
#ifdef FOG_VOLUMES
        sharedFogVolumeMask = 0u;
#endif
    }

    barrier();

    for (int i = int(gl_LocalInvocationIndex); i < pushConstants.lightCount; i += int(gl_WorkGroupSize.x)) {
        VolumetricLight light = volumetricLights[i];
        float radius = light.direction.w;
        uint lightType = floatBitsToUint(light.color.a);

        Sphere sphere;
        if (lightType == POINT_LIGHT) {
            sphere.center = light.location.xyz;
            sphere.radius = radius;
        }
        else if (lightType == SPOT_LIGHT) {
            sphere = CalculateSphereFromSpotLight(light.location, light.direction, radius);
        }
        else {
            continue;
        }

        if (SphereAABBIntersection(sphere, aabb) && SphereFrustumIntersection(sphere, planes)) {
            int lightIdx = atomicAdd(sharedLightCount, 1);
            if (lightIdx < lightCountPerCluster)
                sharedLightIndices[lightIdx] = i;
        }
    }

#ifdef FOG_VOLUMES
    vec3 clusterCenter = vec3(0.0);
    for (int i = 0; i < 8; i++)
        clusterCenter += corners[i] / 8.0;

    float clusterRadius = 0.0;
    for (int i = 0; i < 8; i++)
        clusterRadius = max(clusterRadius, distance(corners[i], clusterCenter));

    vec3 worldClusterCenter = vec3(globalData.ivMatrix * vec4(clusterCenter, 1.0));

    for (int i = int(gl_LocalInvocationIndex); i < pushConstants.fogVolumeCount; i += int(gl_WorkGroupSize.x)) {
        FogVolume volume = fogVolumes[i];

        // Each row is the gradient of a unit axis, this also keeps flat and long volumes tight
        vec3 unitCenter = GetFogVolumeUnitPosition(volume, worldClusterCenter);
        vec3 unitRadius = clusterRadius * vec3(length(volume.worldToUnit0.xyz),
            length(volume.worldToUnit1.xyz), length(volume.worldToUnit2.xyz));

        if (all(lessThanEqual(abs(unitCenter), vec3(1.0) + unitRadius)))
            atomicOr(sharedFogVolumeMask, 1u << uint(i));
    }
#endif

    barrier();

    int lightCount = min(lightCountPerCluster, sharedLightCount);
    int clusterOffset = int(cluster.x + (cluster.y + cluster.z * gl_NumWorkGroups.y) * gl_NumWorkGroups.x) *
        (lightCountPerCluster + 1);
    for (int i = int(gl_LocalInvocationIndex); i < lightCount; i += int(gl_WorkGroupSize.x)) {
        lightIndices[clusterOffset + i + 1] = sharedLightIndices[i];
    }

    if (gl_LocalInvocationIndex == 0u) {
        lightIndices[clusterOffset] = lightCount;
#ifdef FOG_VOLUMES
        fogVolumeMasks[GetFogVolumeClusterIndex()] = sharedFogVolumeMask;
#endif
    }

}
