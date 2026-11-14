#include <common.hsh>

// Writes the visible instances of all views into the regions of their meshes. Reversed winding instances fill a
// region from its end, impostors have their own regions
// The main view draws the instances visible in the last frame first, OCCLUSION then tests the others against that
// depth, writes them into their own view and remembers the visibility for the next frame

#ifdef OCCLUSION
#include <../globals.hsh>
#endif

layout (local_size_x = 64) in;

#ifdef OCCLUSION
layout(std430, set = 3, binding = 0) buffer Instances {
#else
layout(std430, set = 3, binding = 0) readonly buffer Instances {
#endif
    Instance instances[];
};

layout(std430, set = 3, binding = 1) readonly buffer Meshes {
    CullMesh meshes[];
};

layout(std430, set = 3, binding = 2) readonly buffer Views {
    CullView views[];
};

layout(std430, set = 3, binding = 3) buffer Counters {
    uint counters[];
};

layout(std430, set = 3, binding = 4) writeonly buffer Indices {
    uint indices[];
};

layout(std430, set = 3, binding = 5) writeonly buffer ImpostorIndices {
    uint impostorIndices[];
};

layout(push_constant) uniform constants {
    vec4 cameraLocation;
    uint instanceCount;
    uint viewCount;
    uint maxRayTracingInstanceCount;
    uint includeObjectHistory;
    // Viewport height in pixels divided by 2 * tan(fov / 2)
    float focalLength;
    // This view only gets the instances which were visible in the last frame
    uint occlusionView;
    uint firstView;
    uint padding0;
    ivec2 depthResolution;
    int hiZLevelCount;
    uint padding1;
} PushConstants;

#ifndef RAY_TRACING_INSTANCES
// The instances of the occluded view within its frustum, starts with the indirect dispatch of the occlusion test
layout(std430, set = 3, binding = 14) buffer OcclusionInstances {
    uint occlusionGroupCountX;
    uint occlusionGroupCountY;
    uint occlusionGroupCountZ;
    uint occlusionInstanceCount;
    uint occlusionInstances[];
};
#endif

#ifdef OCCLUSION
// Hierarchical depth with the farthest depth in g
layout(set = 3, binding = 13) uniform sampler2D hiZTexture;

bool IsOccluded(vec3 aabbMin, vec3 aabbMax) {

    vec2 ndcMin = vec2(1.0);
    vec2 ndcMax = vec2(-1.0);
    float closestDepth = 1.0;

    for (int i = 0; i < 8; i++) {
        vec3 corner = vec3((i & 1) != 0 ? aabbMax.x : aabbMin.x, (i & 2) != 0 ? aabbMax.y : aabbMin.y,
            (i & 4) != 0 ? aabbMax.z : aabbMin.z);
        vec4 clip = globalData.pvMatrixCurrent * vec4(corner, 1.0);
        // Bounds crossing the near plane can't be projected
        if (clip.w <= 0.0 || clip.z <= 0.0)
            return false;

        vec3 ndc = clip.xyz / clip.w;
        ndcMin = min(ndcMin, ndc.xy);
        ndcMax = max(ndcMax, ndc.xy);
        closestDepth = min(closestDepth, ndc.z);
    }

    // One pixel more, the pixels at the border of the bounds are only partly covered
    ivec2 maxPixel = PushConstants.depthResolution - 1;
    ivec2 pixelMin = clamp(ivec2((ndcMin * 0.5 + 0.5) * vec2(PushConstants.depthResolution)) - 1, ivec2(0), maxPixel);
    ivec2 pixelMax = clamp(ivec2((ndcMax * 0.5 + 0.5) * vec2(PushConstants.depthResolution)) + 1, ivec2(0), maxPixel);

    // The bounds cover at most three texels per axis in this level
    ivec2 size = pixelMax - pixelMin;
    int level = findMSB(max(max(size.x, size.y), 1));
    if (level >= PushConstants.hiZLevelCount)
        return false;

    ivec2 texelMin = pixelMin >> level;
    ivec2 texelMax = pixelMax >> level;
    for (int y = texelMin.y; y <= texelMax.y; y++) {
        for (int x = texelMin.x; x <= texelMax.x; x++) {
            if (closestDepth <= texelFetch(hiZTexture, ivec2(x, y), level).g)
                return false;
        }
    }

    return true;

}
#endif

#ifdef RAY_TRACING_INSTANCES
#include <rtinstances.hsh>
#elif !defined(OCCLUSION)
// Closest distances (float bits) per mesh for the streaming: drawn instances to their bounds divided by their scale
// for the textures, all instances to their positions or centers for the meshes
layout(std430, set = 3, binding = 12) buffer MeshDistances {
    uint meshDistances[];
};

void WriteMeshDistance(uint idx, float distance) {

    uint distanceBits = floatBitsToUint(distance);
    // Most instances are further away than the closest one, reading first skips their atomics
    if (distanceBits < meshDistances[idx])
        atomicMin(meshDistances[idx], distanceBits);

}
#endif

bool Intersects(vec3 aabbMin, vec3 aabbMax, CullView view) {

    for (int i = 0; i < 6; i++) {
        vec4 plane = view.planes[i];
        vec3 s = mix(aabbMin, aabbMax, greaterThanEqual(plane.xyz, vec3(0.0)));
        if (plane.w + dot(plane.xyz, s) < 0.0)
            return false;
    }

    return true;

}

float DistanceSqr(vec3 a, vec3 b) {

    vec3 diff = a - b;
    return dot(diff, diff);

}

void main() {

#ifdef OCCLUSION
    if (gl_GlobalInvocationID.x >= occlusionInstanceCount)
        return;
    uint idx = occlusionInstances[gl_GlobalInvocationID.x];
#else
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= PushConstants.instanceCount)
        return;
#endif

    Instance instance = instances[idx];
    bool dontCull = (instance.flags & INSTANCE_DONT_CULL) != 0u;
    if ((instance.flags & INSTANCE_VISIBLE) == 0u && !dontCull)
        return;

    CullMesh mesh = meshes[instance.meshIdx];

    vec3 localCenter = 0.5 * (mesh.aabbMin.xyz + mesh.aabbMax.xyz);
    vec3 localExtent = 0.5 * (mesh.aabbMax.xyz - mesh.aabbMin.xyz);

    mat3x4 matrix = instance.matrix;
    vec3 center = vec4(localCenter, 1.0) * matrix;
    vec3 extent = vec3(dot(abs(matrix[0].xyz), localExtent), dot(abs(matrix[1].xyz), localExtent),
        dot(abs(matrix[2].xyz), localExtent));

    vec3 cameraLocation = PushConstants.cameraLocation.xyz;
    float centerDistanceSqr = DistanceSqr(center, cameraLocation);
    float positionDistanceSqr = DistanceSqr(GetInstancePosition(matrix), cameraLocation);

    bool geometry = (mesh.flags & MESH_GEOMETRY) != 0u;

    // Instances smaller than the minimum screen size are culled, from the ray traced scene as well
    float screenSizeSqr = 4.0 * dot(extent, extent) * PushConstants.focalLength * PushConstants.focalLength;
    float minScreenSizeSqr = mesh.minScreenSize * mesh.minScreenSize * centerDistanceSqr;
    bool largeEnough = dontCull || screenSizeSqr >= minScreenSizeSqr;

#ifdef RAY_TRACING_INSTANCES
    if ((mesh.flags & MESH_DRAWABLE) == 0u || !largeEnough)
        return;

    if ((instance.flags & INSTANCE_VISIBLE) != 0u)
        WriteRayTracingInstance(instance, positionDistanceSqr, PushConstants.maxRayTracingInstanceCount,
            PushConstants.includeObjectHistory > 0u);
#else
#ifndef OCCLUSION
    // The streaming hears of instances from half the minimum screen size on, so their meshes are resident once they
    // get big enough
    bool requested = dontCull || 4.0 * screenSizeSqr >= minScreenSizeSqr;

    // Meshes which aren't drawable need the distances as well, to be loaded again. Instances that are never culled
    // need their geometry at any distance
    if (requested)
        WriteMeshDistance(2u * instance.meshIdx + 1u, dontCull ? 0.0 : sqrt(min(positionDistanceSqr, centerDistanceSqr)));

    bool withinCullingDistance = dontCull || centerDistanceSqr <= mesh.aabbMin.w * mesh.aabbMin.w;

    // Instances drawn as impostors or culled by distance don't need the textures of the mesh
    bool impostorOnly = (mesh.flags & MESH_IMPOSTOR) != 0u &&
        (!geometry || positionDistanceSqr >= mesh.impostorDistance * mesh.impostorDistance);
    if (geometry && largeEnough && !impostorOnly && withinCullingDistance) {
        float scale = max(length(vec3(matrix[0].x, matrix[1].x, matrix[2].x)),
            max(length(vec3(matrix[0].y, matrix[1].y, matrix[2].y)), length(vec3(matrix[0].z, matrix[1].z, matrix[2].z))));
        WriteMeshDistance(2u * instance.meshIdx, length(max(abs(cameraLocation - center) - extent, vec3(0.0))) / scale);
    }
#endif

    if ((mesh.flags & MESH_DRAWABLE) == 0u || !largeEnough)
        return;
#endif

    bool visibleLastFrame = instance.visibleLastFrame > 0u;
    bool drawnEarly = (mesh.flags & MESH_DRAW_EARLY) != 0u && (visibleLastFrame || dontCull);
    for (uint i = PushConstants.firstView; i < PushConstants.firstView + PushConstants.viewCount; i++) {
        CullView view = views[i];
        bool shadow = view.shadow > 0u;
        if (shadow && (mesh.flags & MESH_CAST_SHADOW) == 0u)
            continue;

        if (!dontCull) {
            float cullingDistance = shadow ? mesh.aabbMax.w : mesh.aabbMin.w;
            if (centerDistanceSqr > cullingDistance * cullingDistance)
                continue;
            if (!Intersects(center - extent, center + extent, view))
                continue;
        }

#ifdef OCCLUSION
        bool visible = dontCull || !IsOccluded(center - extent, center + extent);
        if (visible != visibleLastFrame)
            instances[idx].visibleLastFrame = visible ? 1u : 0u;
        if (!visible || drawnEarly)
            continue;
#elif !defined(RAY_TRACING_INSTANCES)
        if (i == PushConstants.occlusionView) {
            uint occlusionIdx = atomicAdd(occlusionInstanceCount, 1u);
            occlusionInstances[occlusionIdx] = idx;
            if (occlusionIdx % 64u == 0u) {
                atomicAdd(occlusionGroupCountX, 1u);
                occlusionGroupCountY = 1u;
                occlusionGroupCountZ = 1u;
            }

            if (!drawnEarly)
                continue;
        }
#endif

        float impostorDistance = shadow ? mesh.impostorShadowDistance : mesh.impostorDistance;
        // Meshes without resident geometry are drawn as impostors only
        bool impostor = (mesh.flags & MESH_IMPOSTOR) != 0u &&
            (!geometry || positionDistanceSqr >= impostorDistance * impostorDistance);

        uint counterIdx = view.counterOffset + instance.meshIdx * DRAW_TYPE_COUNT;
        if (impostor) {
            uint slot = atomicAdd(counters[counterIdx + DRAW_IMPOSTOR], 1u);
            if (slot < mesh.regionCapacity)
                impostorIndices[view.impostorIndexOffset + mesh.impostorRegionOffset + slot] = idx;
        }
        else if ((instance.flags & INSTANCE_REVERSED_WINDING) != 0u) {
            uint slot = atomicAdd(counters[counterIdx + DRAW_REVERSED_WINDING], 1u);
            if (slot < mesh.regionCapacity)
                indices[view.indexOffset + mesh.regionOffset + mesh.regionCapacity - 1u - slot] = idx;
        }
        else {
            uint slot = atomicAdd(counters[counterIdx + DRAW_DEFAULT_WINDING], 1u);
            if (slot < mesh.regionCapacity)
                indices[view.indexOffset + mesh.regionOffset + slot] = idx;
        }
    }

}
