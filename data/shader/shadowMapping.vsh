#include <globals.hsh>
#include <wind.hsh>

layout(location=0) in vec3 vPosition;
#ifdef MATERIAL_GRAPH
layout(location=1) in vec4 vNormal;
#ifdef TANGENTS
layout(location=3) in vec4 vTangent;
#endif
#ifdef VERTEX_COLORS
layout(location=4) in vec4 vVertexColors;
#endif
#endif
#if defined(OPACITY_MAP) || (defined(MATERIAL_GRAPH) && defined(TEX_COORDS))
layout(location=2) in vec2 vTexCoord;
#endif
#ifdef SKINNED
layout(location=5) in uvec4 vJoints;
layout(location=6) in vec4 vWeights;
#endif

#if defined(OPACITY_MAP) || defined(MATERIAL_GRAPH_SURFACE)
layout(location=0) out vec2 texCoordVS;
#endif

#ifdef MATERIAL_GRAPH_SURFACE
layout(location=1) out vec3 worldPositionVS;
layout(location=2) out vec3 worldNormalVS;
layout(location=3) out vec4 vertexColorsVS;
layout(location=10) out vec3 objectPositionVS;
layout(location=11) out vec3 instancePositionVS;
layout(location=12) out vec3 objectNormalVS;
#endif

#ifdef INSTANCE_BUFFER
#include <culling/instances.hsh>
#else
layout(std430, set = 1, binding = 1) readonly buffer Matrices {
    mat3x4 matrices[];
};
#endif

#ifdef SKINNED
layout(std430, set = 1, binding = 4) readonly buffer JointMatrices {
    mat3x4 jointMatrices[];
};
#endif

layout(set = 3, binding = 0) uniform sampler2D windNoiseMap;

layout(push_constant) uniform constants {
    mat4 lightSpaceMatrix;
    uint vegetation;
    uint invertUVs;
    float windTextureLod;
    float windBendScale;
    float windWiggleScale;
    uint textureID;
    uint jointOffset;
    uint jointCount;
    uint instanceOffset;
#ifdef MATERIAL_GRAPH
    uint materialGraphOffset;
#endif
} PushConstants;

void main() {

#ifdef INSTANCE_BUFFER
    mat4 mMatrix = GetInstanceMatrix(instanceIndices[gl_InstanceIndex]);
#else
    mat4 mMatrix = mat4(transpose(matrices[gl_InstanceIndex]));
#endif
    
#ifdef OPACITY_MAP
    texCoordVS = PushConstants.invertUVs > 0 ? vec2(vTexCoord.x, 1.0 - vTexCoord.y) : vTexCoord;
#endif

    mat4 matrix = mMatrix;
    vec3 position = vPosition;

#ifdef SKINNED
    uint jointOffset = PushConstants.jointOffset + (gl_InstanceIndex - PushConstants.instanceOffset) * PushConstants.jointCount;
    vec4 weights = vWeights / dot(vWeights, vec4(1.0));

    mat3x4 skinMatrix = weights.x * jointMatrices[jointOffset + vJoints.x] +
        weights.y * jointMatrices[jointOffset + vJoints.y] +
        weights.z * jointMatrices[jointOffset + vJoints.z] +
        weights.w * jointMatrices[jointOffset + vJoints.w];

    position = vec4(vPosition, 1.0) * skinMatrix;
#endif

#ifdef MATERIAL_GRAPH
    vec3 graphNormal = vNormal.xyz;
    vec2 graphTexCoord = vec2(0.0);
    vec3 graphTangent = vec3(1.0, 0.0, 0.0);
    vec4 graphColor = vec4(1.0);
#ifdef SKINNED
    graphNormal = vec4(graphNormal, 0.0) * skinMatrix;
#endif
#ifdef TEX_COORDS
    graphTexCoord = PushConstants.invertUVs > 0 ? vec2(vTexCoord.x, 1.0 - vTexCoord.y) : vTexCoord;
#endif
#ifdef TANGENTS
    graphTangent = vTangent.xyz;
#endif
#ifdef VERTEX_COLORS
    graphColor = vVertexColors;
#endif

    MaterialVertexInput graphInput = CreateMaterialVertexInput(vPosition, position, graphNormal, graphTangent, graphTexCoord, graphColor,
        mMatrix, globalData.time, PushConstants.materialGraphOffset);
    WriteMaterialVaryings(graphInput);

#ifdef MATERIAL_GRAPH_SURFACE
    texCoordVS = graphTexCoord;
    objectPositionVS = vPosition;
    objectNormalVS = graphNormal;
    instancePositionVS = vec3(mMatrix[3]);
    worldNormalVS = graphInput.worldNormal;
    vertexColorsVS = graphColor;
#endif

#ifdef MATERIAL_GRAPH_VERTEX_OFFSET
    position += EvaluateMaterialVertexOffset(graphInput);
#endif

#ifdef MATERIAL_GRAPH_SURFACE
    worldPositionVS = vec3(mMatrix * vec4(position, 1.0));
#endif
#endif

    if (PushConstants.vegetation > 0) {

        // This is not well optimized and could use some work
        vec3 windPosition = vec3(mMatrix * vec4(position, 1.0));
        vec2 windDir = normalize((inverse(mat3(mMatrix)) * vec3(globalData.windDir.x, 0.0, globalData.windDir.y)).xz);
        position = WindAnimation(windNoiseMap, position, windDir, PushConstants.windBendScale,
            PushConstants.windWiggleScale, PushConstants.windTextureLod, globalData.time, windPosition);

    }
    
    gl_Position = PushConstants.lightSpaceMatrix * matrix * vec4(position, 1.0f);
    
}