#include <skinning.hsh>

layout(buffer_reference, std430, buffer_reference_align = 4) writeonly buffer PositionBuffer {
    float positions[];
};

void main() {

    SkinJob job;
    uint idx;
    if (!GetSkinJob(job, idx))
        return;

    SkinVertex skinVertex = SkinVertexBuffer(job.skinVertices).vertices[idx];
    mat3x4 skinMatrix = GetSkinMatrix(skinVertex.skin, job.jointOffset);

    vec3 position = vec4(skinVertex.position.xyz, 1.0) * skinMatrix;

    PositionBuffer positions = PositionBuffer(job.target);
    positions.positions[idx * 3 + 0] = position.x;
    positions.positions[idx * 3 + 1] = position.y;
    positions.positions[idx * 3 + 2] = position.z;

}