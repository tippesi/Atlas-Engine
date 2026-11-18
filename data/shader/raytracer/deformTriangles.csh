// Gathers the vertices deformed by deform.csh into the triangles of a software BLAS, hardware ray tracing builds
// its BLAS from the deformed positions directly
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require

#include <structures.hsh>
#include <../common/packing.hsh>

// Has to match the group size of the triangle pass in RayTracingWorld
layout (local_size_x = 64) in;

// Has to match GPUDeformTriangleJob in RTStructures.h
struct DeformTriangleJob {
    uvec2 triangleVertices;
    uvec2 positions;
    uvec2 normals;
    uvec2 triangles;
    uvec2 bvhTriangles;
    uint firstGroup;
    uint triangleCount;
};

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer TriangleVertexBuffer {
    uvec4 triangleVertices[];
};

layout(buffer_reference, std430, buffer_reference_align = 4) readonly buffer PositionBuffer {
    float positions[];
};

layout(buffer_reference, std430, buffer_reference_align = 8) readonly buffer NormalBuffer {
    uvec2 normals[];
};

layout(buffer_reference, std430, buffer_reference_align = 16) buffer TriangleBuffer {
    PackedTriangle triangles[];
};

layout(buffer_reference, std430, buffer_reference_align = 16) buffer BVHTriangleBuffer {
    PackedBVHTriangle bvhTriangles[];
};

layout(std430, set = 3, binding = 0) readonly buffer TriangleJobs {
    DeformTriangleJob triangleJobs[];
};

layout(std430, set = 3, binding = 2) readonly buffer GroupJobs {
    uint groupJobs[];
};

layout(push_constant) uniform constants {
    uint groupOffset;
    uint groupCount;
} PushConstants;

void main() {

    uint group = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;
    if (group >= PushConstants.groupCount)
        return;

    DeformTriangleJob job = triangleJobs[groupJobs[PushConstants.groupOffset + group]];
    uint idx = (group - job.firstGroup) * gl_WorkGroupSize.x + gl_LocalInvocationIndex;
    if (idx >= job.triangleCount)
        return;

    PositionBuffer deformedPositions = PositionBuffer(job.positions);
    NormalBuffer deformedNormals = NormalBuffer(job.normals);
    uvec4 vertexIndices = TriangleVertexBuffer(job.triangleVertices).triangleVertices[idx];

    vec3 positions[3];
    vec3 normals[3];
    for (int i = 0; i < 3; i++) {
        uint vertexIdx = vertexIndices[i];
        positions[i] = vec3(deformedPositions.positions[vertexIdx * 3u], deformedPositions.positions[vertexIdx * 3u + 1u],
            deformedPositions.positions[vertexIdx * 3u + 2u]);
        normals[i] = unpackUnitVector(int(deformedNormals.normals[vertexIdx].x)).xyz;
    }

    TriangleBuffer triangles = TriangleBuffer(job.triangles);
    vec4 texCoords = triangles.triangles[idx].d0;
    vec2 uv0 = unpackHalf2x16(floatBitsToUint(texCoords.x));
    vec2 uv1 = unpackHalf2x16(floatBitsToUint(texCoords.y));
    vec2 uv2 = unpackHalf2x16(floatBitsToUint(texCoords.z));

    // Same tangent frame as the triangles built on the CPU
    vec3 v0v1 = positions[1] - positions[0];
    vec3 v0v2 = positions[2] - positions[0];
    vec2 uv0uv1 = uv1 - uv0;
    vec2 uv0uv2 = uv2 - uv0;

    float r = 1.0 / (uv0uv1.x * uv0uv2.y - uv0uv2.x * uv0uv1.y);

    vec3 s = (uv0uv2.y * v0v1 - uv0uv1.y * v0v2) * r;
    vec3 t = (uv0uv1.x * v0v2 - uv0uv2.x * v0v1) * r;

    vec3 normal = normalize(normals[0] + normals[1] + normals[2]);

    vec3 tangent = normalize(s - normal * dot(normal, s));
    float handedness = dot(cross(tangent, normal), t) < 0.0 ? 1.0 : -1.0;
    vec3 bitangent = handedness * normalize(cross(tangent, normal));

    triangles.triangles[idx].v0 = vec4(positions[0], intBitsToFloat(packUnitVector(normals[0])));
    triangles.triangles[idx].v1 = vec4(positions[1], intBitsToFloat(packUnitVector(normals[1])));
    triangles.triangles[idx].v2 = vec4(positions[2], intBitsToFloat(packUnitVector(normals[2])));
    triangles.triangles[idx].d1.xy = vec2(intBitsToFloat(packUnitVector(tangent)), intBitsToFloat(packUnitVector(bitangent)));

    BVHTriangleBuffer bvhTriangles = BVHTriangleBuffer(job.bvhTriangles);
    bvhTriangles.bvhTriangles[idx].v0.xyz = positions[0];
    bvhTriangles.bvhTriangles[idx].v1.xyz = positions[1];
    bvhTriangles.bvhTriangles[idx].v2.xyz = positions[2];

}
