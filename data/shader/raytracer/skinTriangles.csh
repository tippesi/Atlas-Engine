#include <structures.hsh>
#include <skinning.hsh>
#include <../common/packing.hsh>

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer TriangleVertexBuffer {
    uvec4 triangleVertices[];
};

layout(buffer_reference, std430, buffer_reference_align = 16) buffer TriangleBuffer {
    PackedTriangle triangles[];
};

// Skins the triangles of software ray tracing, hardware ray tracing deforms the vertices in deform.csh
layout(buffer_reference, std430, buffer_reference_align = 16) buffer BVHTriangleBuffer {
    PackedBVHTriangle bvhTriangles[];
};

void main() {

    SkinJob job;
    uint idx;
    if (!GetSkinJob(job, idx))
        return;

    SkinVertexBuffer skinVertices = SkinVertexBuffer(job.skinVertices);
    TriangleBuffer triangles = TriangleBuffer(job.target);
    uvec4 vertexIndices = TriangleVertexBuffer(job.triangleVertices).triangleVertices[idx];

    vec3 positions[3];
    vec3 normals[3];
    for (int i = 0; i < 3; i++) {
        SkinVertex skinVertex = skinVertices.vertices[vertexIndices[i]];
        mat3x4 skinMatrix = GetSkinMatrix(skinVertex.skin, job.jointOffset);

        vec4 position = skinVertex.position;
        positions[i] = vec4(position.xyz, 1.0) * skinMatrix;
        vec3 normal = unpackUnitVector(floatBitsToInt(position.w)).xyz;
        normals[i] = normalize(vec4(normal, 0.0) * skinMatrix);
    }

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
