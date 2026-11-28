// Deforms vertices for hardware ray tracing like deferred/geometry.vsh (skinning, graph vertex offset, wind) into the
// positions of the instance BLAS and the normals the hits read. Material graphs include it after their vertex functions
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require

#include <../globals.hsh>
#include <../wind.hsh>
#include <../common/packing.hsh>
#include <../common/rotation.hsh>
#include <../common/texCoord.hsh>

// Has to match the group size of the deformation pass in RayTracingWorld
layout (local_size_x = 64) in;

// Has to match GPUDeformVertex in RTStructures.h
struct DeformVertex {
    // Position and the packed normal in w
    vec4 position;
    // Packed tangent with the handedness in w
    uint tangent;
    uint texCoord;
    uint color;
    uint weights;
    uint joints01;
    uint joints23;
    uint texCoord1;
    uint padding0;
};

// One deformed vertex range of an instance, has to match GPUDeformJob in RTStructures.h
struct DeformJob {
    mat3x4 matrix;
    uvec2 sourceVertices;
    uvec2 positions;
    uvec2 normals;
    uint firstGroup;
    uint vertexOffset;
    uint vertexCount;
    uint deformOffset;
    float windBendScale;
    float windWiggleScale;
    float windTextureLod;
    uint materialGraphOffset;
    uint invertUVs;
    float uvTiling;
    vec2 uvAnimation;
    uint hasTexCoords1;
    uint padding0;
};

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer DeformVertexBuffer {
    DeformVertex vertices[];
};

layout(buffer_reference, std430, buffer_reference_align = 4) writeonly buffer PositionBuffer {
    float positions[];
};

layout(buffer_reference, std430, buffer_reference_align = 8) writeonly buffer NormalBuffer {
    uvec2 normals[];
};

layout(std430, set = 3, binding = 0) readonly buffer DeformJobs {
    DeformJob deformJobs[];
};

layout(std430, set = 3, binding = 1) readonly buffer JointMatrices {
    mat3x4 jointMatrices[];
};

layout(std430, set = 3, binding = 2) readonly buffer GroupJobs {
    uint groupJobs[];
};

#ifdef WIND
layout(set = 3, binding = 3) uniform sampler2D windNoiseMap;
#endif

#ifdef SOFT_BODY
// Positions with the packed normal in w
layout(std430, set = 3, binding = 4) readonly buffer SoftBodyVertices {
    vec4 softBodyVertices[];
};
#endif

layout(push_constant) uniform constants {
    uint groupOffset;
    uint groupCount;
} PushConstants;

void main() {

    uint group = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;
    if (group >= PushConstants.groupCount)
        return;

    DeformJob job = deformJobs[groupJobs[PushConstants.groupOffset + group]];
    uint idx = (group - job.firstGroup) * gl_WorkGroupSize.x + gl_LocalInvocationIndex;
    if (idx >= job.vertexCount)
        return;

    uint vertexIdx = job.vertexOffset + idx;
    DeformVertex vertex = DeformVertexBuffer(job.sourceVertices).vertices[vertexIdx];

    vec3 position = vertex.position.xyz;
    vec3 normal = unpackUnitVector(floatBitsToInt(vertex.position.w)).xyz;
    vec4 tangent = unpackUnitVector(int(vertex.tangent));

    mat4 mMatrix = mat4(transpose(job.matrix));

#ifdef SKINNED
    uvec4 joints = uvec4(vertex.joints01 & 0xFFFFu, vertex.joints01 >> 16u, vertex.joints23 & 0xFFFFu, vertex.joints23 >> 16u);
    vec4 weights = unpackUnorm4x8(vertex.weights);
    weights /= dot(weights, vec4(1.0));

    mat3x4 skinMatrix = weights.x * jointMatrices[job.deformOffset + joints.x] +
        weights.y * jointMatrices[job.deformOffset + joints.y] +
        weights.z * jointMatrices[job.deformOffset + joints.z] +
        weights.w * jointMatrices[job.deformOffset + joints.w];

    position = vec4(position, 1.0) * skinMatrix;
    normal = vec4(normal, 0.0) * skinMatrix;
    tangent.xyz = vec4(tangent.xyz, 0.0) * skinMatrix;
#endif

#ifdef SOFT_BODY
    // The vertices of the body start at the joint offset
    vec4 softBodyVertex = softBodyVertices[job.deformOffset + vertexIdx];
    vec3 restNormal = normal;
    position = softBodyVertex.xyz;
    normal = unpackUnitVector(floatBitsToInt(softBodyVertex.w)).xyz;
    tangent.xyz = RotateLike(tangent.xyz, normalize(restNormal), normal);
#endif

#ifdef MATERIAL_GRAPH_VERTEX_OFFSET
    vec2 texCoord = unpackHalf2x16(vertex.texCoord);
    texCoord = TransformTexCoord(texCoord, job.invertUVs > 0u, job.uvAnimation, job.uvTiling);

    // Meshes without tangents use the same default as the rasterizer
    vec3 graphTangent = dot(tangent.xyz, tangent.xyz) > 0.01 ? tangent.xyz : vec3(1.0, 0.0, 0.0);

    // Zero without a second set, like the rasterizer
    vec2 texCoord1 = unpackHalf2x16(vertex.texCoord1);
    texCoord1 = job.invertUVs > 0u && job.hasTexCoords1 > 0u ? vec2(texCoord1.x, 1.0 - texCoord1.y) : texCoord1;

    MaterialVertexInput graphInput = CreateMaterialVertexInput(vertex.position.xyz, position, normal, graphTangent,
        texCoord, texCoord1, unpackUnorm4x8(vertex.color), mMatrix, globalData.time, job.materialGraphOffset);
    normal = EvaluateMaterialVertexNormal(graphInput);
    position += EvaluateMaterialVertexOffset(graphInput);
#endif

#ifdef WIND
    vec3 windPosition = vec3(mMatrix * vec4(position, 1.0));
    position = WindAnimation(windNoiseMap, position, GetWindDirection(mMatrix), job.windBendScale,
        job.windWiggleScale, job.windTextureLod, globalData.time, windPosition);
#endif

    PositionBuffer positions = PositionBuffer(job.positions);
    positions.positions[vertexIdx * 3u + 0u] = position.x;
    positions.positions[vertexIdx * 3u + 1u] = position.y;
    positions.positions[vertexIdx * 3u + 2u] = position.z;

    float tangentLength = length(tangent.xyz);
    tangent.xyz = tangentLength > 0.0 ? tangent.xyz / tangentLength : vec3(0.0);
    NormalBuffer(job.normals).normals[vertexIdx] = uvec2(packUnitVector(normalize(normal)), packUnitVector(tangent));

}
