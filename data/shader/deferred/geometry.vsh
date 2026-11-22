#include <../wind.hsh>
#include <../globals.hsh>
#include <../common/packing.hsh>
#include <../common/rotation.hsh>
#include <../common/texCoord.hsh>
#include <../common/opacity.hsh>
#include <geometry.hsh>

// New naming convention:
// Vertex attributes v[Name]
// Fragment attributes f[Name]
// Etc.
// Vertex stage out paramters [name]VS
// Fragment stage out paramters [name]FS
// Etc.
// Uniform names: camelCase (variables in general)

// Per vertex attributes
layout(location=0) in vec3 vPosition;
layout(location=1) in vec4 vNormal;
#ifdef TEX_COORDS
layout(location=2) in vec2 vTexCoord;
#endif

#ifdef TANGENTS
layout(location=3) in vec4 vTangent;
#endif

#ifdef VERTEX_COLORS
layout(location=4) in vec4 vVertexColors;
#endif

#ifdef SKINNED
layout(location=5) in uvec4 vJoints;
layout(location=6) in vec4 vWeights;
#endif

#ifdef TEX_COORDS_1
layout(location=7) in vec2 vTexCoord1;
#endif

#ifdef INSTANCE_BUFFER
#include <../culling/instances.hsh>
#else
layout(std430, set = 1, binding = 1) readonly buffer CurrentMatrices {
    mat3x4 currentMatrices[];
};

layout(std430, set = 1, binding = 2) readonly buffer LastMatrices {
    mat3x4 lastMatrices[];
};
#endif

#ifdef SKINNED
layout(std430, set = 1, binding = 4) readonly buffer CurrentJointMatrices {
    mat3x4 currentJointMatrices[];
};

layout(std430, set = 1, binding = 5) readonly buffer LastJointMatrices {
    mat3x4 lastJointMatrices[];
};
#endif

#ifdef SOFT_BODY
// Positions with the packed normal in w, each instance has the offset of its vertices
layout(std430, set = 1, binding = 24) readonly buffer CurrentSoftBodyVertices {
    vec4 currentSoftBodyVertices[];
};

layout(std430, set = 1, binding = 25) readonly buffer LastSoftBodyVertices {
    vec4 lastSoftBodyVertices[];
};

layout(std430, set = 1, binding = 26) readonly buffer SoftBodyVertexOffsets {
    uint softBodyVertexOffsets[];
};
#endif

// Vertex out parameters
layout(location=0) out vec3 positionVS;
layout(location=1) out vec3 normalVS;
#ifdef TEX_COORDS
layout(location=2) out vec2 texCoordVS;
#endif
layout(location=3) out vec3 ndcCurrentVS;
layout(location=4) out vec3 ndcLastVS;

#ifdef VERTEX_COLORS
layout(location=5) out vec4 vertexColorsVS;
#endif

// Matrix is several locations
#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
layout(location=7) out mat3 TBN;
#endif

#ifdef MATERIAL_GRAPH_SURFACE
layout(location=10) out vec3 objectPositionVS;
layout(location=11) out vec3 instancePositionVS;
layout(location=12) out vec3 objectNormalVS;
#endif

layout(location=13) flat out uint lodVS;
#ifdef DRAW_DATA
layout(location=14) flat out uint drawIdxVS;
#endif

layout(set = 3, binding = 8) uniform sampler2D windNoiseMap;

// Functions
void main() {

#ifdef DRAW_DATA
    drawIdx = PushConstants.firstDraw + uint(gl_DrawID);
    drawIdxVS = drawIdx;
    lodVS = PushConstants.draw.lod + DrawValue(lod);
#else
    lodVS = PushConstants.draw.lod + uint(gl_DrawID);
#endif

#ifdef INSTANCE_BUFFER
    uint instanceIdx = instanceIndices[gl_InstanceIndex];
    mat4 mMatrix = GetInstanceMatrix(instanceIdx);
    mat4 mMatrixLast = DrawValue(staticMesh) > 0 ? mMatrix : GetLastInstanceMatrix(instanceIdx);
#else
    mat4 mMatrix = mat4(transpose(currentMatrices[gl_InstanceIndex]));
    mat4 mMatrixLast = DrawValue(staticMesh) > 0 ? mMatrix : mat4(transpose(lastMatrices[gl_InstanceIndex]));
#endif

#ifdef TEX_COORDS
    texCoordVS = TransformTexCoord(vTexCoord, DrawValue(invertUVs) > 0,
        vec2(DrawValue(uvAnimationX), DrawValue(uvAnimationY)), DrawValue(uvTiling));
#endif
    
    mat4 mvMatrix = globalData.vMatrix * mMatrix;

    vec3 position = vPosition;
    vec3 lastPosition = vPosition;
    vec3 normal = vNormal.xyz;
#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
    vec3 tangent = vTangent.xyz;
#endif

#ifdef SKINNED
    uint jointOffset = PushConstants.deformOffset + (gl_InstanceIndex - PushConstants.instanceOffset) * PushConstants.jointCount;
    vec4 weights = vWeights / dot(vWeights, vec4(1.0));

    mat3x4 skinMatrix = weights.x * currentJointMatrices[jointOffset + vJoints.x] +
        weights.y * currentJointMatrices[jointOffset + vJoints.y] +
        weights.z * currentJointMatrices[jointOffset + vJoints.z] +
        weights.w * currentJointMatrices[jointOffset + vJoints.w];
    mat3x4 lastSkinMatrix = weights.x * lastJointMatrices[jointOffset + vJoints.x] +
        weights.y * lastJointMatrices[jointOffset + vJoints.y] +
        weights.z * lastJointMatrices[jointOffset + vJoints.z] +
        weights.w * lastJointMatrices[jointOffset + vJoints.w];

    position = vec4(vPosition, 1.0) * skinMatrix;
    lastPosition = vec4(vPosition, 1.0) * lastSkinMatrix;
    normal = vec4(normal, 0.0) * skinMatrix;
#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
    tangent = vec4(tangent, 0.0) * skinMatrix;
#endif
#endif

#ifdef SOFT_BODY
    uint softBodyVertex = softBodyVertexOffsets[PushConstants.deformOffset + gl_InstanceIndex -
        PushConstants.instanceOffset] + gl_VertexIndex;
    vec4 softBodyCurrent = currentSoftBodyVertices[softBodyVertex];
    position = softBodyCurrent.xyz;
    lastPosition = lastSoftBodyVertices[softBodyVertex].xyz;
    normal = unpackUnitVector(floatBitsToInt(softBodyCurrent.w)).xyz;
#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
    // Turned like the normal, so it stays along the surface while the cloth turns
    tangent = RotateLike(tangent, normalize(vNormal.xyz), normal);
#endif
#endif

#ifdef MATERIAL_GRAPH
    vec2 graphTexCoord = vec2(0.0);
    vec2 graphTexCoord1 = vec2(0.0);
    vec3 graphTangent = vec3(1.0, 0.0, 0.0);
    vec4 graphColor = vec4(1.0);
#ifdef TEX_COORDS
    graphTexCoord = texCoordVS;
#endif
#ifdef TEX_COORDS_1
    graphTexCoord1 = DrawValue(invertUVs) > 0 ? vec2(vTexCoord1.x, 1.0 - vTexCoord1.y) : vTexCoord1;
#endif
#ifdef TANGENTS
    graphTangent = vTangent.xyz;
#endif
#ifdef VERTEX_COLORS
    graphColor = vVertexColors;
#endif

    MaterialVertexInput graphInput = CreateMaterialVertexInput(vPosition, position, normal, graphTangent, graphTexCoord, graphTexCoord1,
        graphColor, mMatrix, globalData.time, PushConstants.materialGraphOffset);

#if defined(MATERIAL_GRAPH_VERTEX_OPACITY) && !defined(REFRACTION)
    // The opacity is the same for the whole draw, moving all vertices outside the view keeps fragments free of discards.
    // Refracting surfaces stay without opacity, they are clear glass
#ifdef TRANSLUCENT_PASS
    if (EvaluateMaterialOpacity(graphInput) <= 0.0) {
#else
    if (EvaluateMaterialOpacity(graphInput) < opacityCutoff) {
#endif
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        return;
    }
#endif

    WriteMaterialVaryings(graphInput);

#ifdef MATERIAL_GRAPH_SURFACE
    // Patterns stay on the surface when it moves
    objectPositionVS = vPosition;
    instancePositionVS = vec3(mMatrix[3]);
#endif

#ifdef MATERIAL_GRAPH_VERTEX_OFFSET
    normal = EvaluateMaterialVertexNormal(graphInput);
    position += EvaluateMaterialVertexOffset(graphInput);

    graphInput = CreateMaterialVertexInput(vPosition, lastPosition, graphInput.normal, graphTangent, graphTexCoord, graphTexCoord1,
        graphColor, mMatrixLast, globalData.time - globalData.deltaTime, PushConstants.materialGraphOffset);
    lastPosition += EvaluateMaterialVertexOffset(graphInput);
#endif

#ifdef MATERIAL_GRAPH_SURFACE
    objectNormalVS = normal;
#endif
#endif

#ifdef VEGETATION
    vec3 windPosition = vec3(mMatrix * vec4(position, 1.0));
    vec2 windDir = GetWindDirection(mMatrix);
    position = WindAnimation(windNoiseMap, position, windDir, DrawValue(windBendScale),
        DrawValue(windWiggleScale), DrawValue(windTextureLod), globalData.time, windPosition);
    lastPosition = WindAnimation(windNoiseMap, lastPosition, windDir, DrawValue(windBendScale),
        DrawValue(windWiggleScale), DrawValue(windTextureLod),
        globalData.time - globalData.deltaTime, windPosition);
#endif

    vec4 positionToCamera = mvMatrix * vec4(position, 1.0);
    positionVS = positionToCamera.xyz;

    gl_Position = globalData.pMatrix * positionToCamera;

    // Needed for velocity buffer calculation 
    ndcCurrentVS = vec3(gl_Position.xy, gl_Position.w);
    // For moving objects we need the last frames matrix
    vec4 last = globalData.pvMatrixLast * mMatrixLast * vec4(lastPosition, 1.0);
    ndcLastVS = vec3(last.xy, last.w);

    // Only after ndc calculation apply the clip correction
    gl_Position = gl_Position;
    
    // The cofactor matrix is the inverse transpose scaled by the determinant. Transforms with reversed winding have a
    // negative determinant, so the orientation flips the normals back to the outside
    mat3 modelView = mat3(mvMatrix);
    mat3 cofactorMatrix = mat3(cross(modelView[1], modelView[2]),
        cross(modelView[2], modelView[0]), cross(modelView[0], modelView[1]));
    float orientation = dot(modelView[0], cofactorMatrix[0]) < 0.0 ? -1.0 : 1.0;
    normalVS = orientation * (cofactorMatrix * normal);

#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
    vec3 normalView = normalize(normalVS);
    float correctionFactor = orientation * vTangent.w * (DrawValue(invertUVs) > 0 ? 1.0 : -1.0);
    vec3 tangentView = mat3(mvMatrix) * tangent;
    tangentView = normalize(tangentView - normalView * dot(tangentView, normalView));
    
    vec3 bitangent = normalize(correctionFactor * 
        cross(tangentView, normalView));

    TBN = mat3(tangentView, bitangent, normalView);
#endif

#ifdef VERTEX_COLORS
    vertexColorsVS = vVertexColors;
#endif
    
}
