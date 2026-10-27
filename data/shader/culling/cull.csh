#include <common.hsh>

// Culls all instances against all views at once and writes the indices of the visible ones into the regions of
// their meshes. Reversed winding instances fill a region from its end, impostors have their own regions.
// With RAY_TRACING_INSTANCES it writes the hardware ray tracing instances as well.

layout (local_size_x = 64) in;

layout(std430, set = 3, binding = 0) readonly buffer Instances {
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
} PushConstants;

#ifdef RAY_TRACING_INSTANCES
#include <rtinstances.hsh>
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

    uint idx = gl_GlobalInvocationID.x;
    if (idx >= PushConstants.instanceCount)
        return;

    Instance instance = instances[idx];
    bool dontCull = (instance.flags & INSTANCE_DONT_CULL) != 0u;
    if ((instance.flags & INSTANCE_VISIBLE) == 0u && !dontCull)
        return;

    CullMesh mesh = meshes[instance.meshIdx];
    if ((mesh.flags & MESH_DRAWABLE) == 0u)
        return;

    vec3 localCenter = 0.5 * (mesh.aabbMin.xyz + mesh.aabbMax.xyz);
    vec3 localExtent = 0.5 * (mesh.aabbMax.xyz - mesh.aabbMin.xyz);

    mat3x4 matrix = instance.matrix;
    vec3 center = vec4(localCenter, 1.0) * matrix;
    vec3 extent = vec3(dot(abs(matrix[0].xyz), localExtent), dot(abs(matrix[1].xyz), localExtent),
        dot(abs(matrix[2].xyz), localExtent));

    vec3 cameraLocation = PushConstants.cameraLocation.xyz;
    float centerDistanceSqr = DistanceSqr(center, cameraLocation);
    float positionDistanceSqr = DistanceSqr(GetInstancePosition(matrix), cameraLocation);

#ifdef RAY_TRACING_INSTANCES
    if ((instance.flags & INSTANCE_VISIBLE) != 0u)
        WriteRayTracingInstance(instance, positionDistanceSqr, PushConstants.maxRayTracingInstanceCount,
            PushConstants.includeObjectHistory > 0u);
#endif

    for (uint i = 0u; i < PushConstants.viewCount; i++) {
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

        float impostorDistance = shadow ? mesh.impostorShadowDistance : mesh.impostorDistance;
        bool impostor = (mesh.flags & MESH_IMPOSTOR) != 0u && positionDistanceSqr >= impostorDistance * impostorDistance;

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
