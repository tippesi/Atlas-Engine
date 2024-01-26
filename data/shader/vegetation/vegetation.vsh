#include <structures.hsh>
#include <../wind.hsh>
#include <../globals.hsh>

// Per vertex attributes
layout(location=0) in vec3 vPosition;
layout(location=1) in vec3 vNormal;
#ifdef TEX_COORDS
layout(location=2) in vec2 vTexCoord;
#endif

#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
layout(location=3) in vec4 vTangent;
#endif

#ifdef VERTEX_COLORS
layout(location=4) in vec4 vVertexColors;
#endif

// Vertex out parameters
#ifdef NORMAl_MAP
layout(location=0) out vec3 positionVS;
#endif
layout(location=1) out vec3 normalVS;
#ifdef TEX_COORDS
layout(location=2) out vec2 texCoordVS;
#endif
layout(location=3) out vec3 ndcCurrentVS;
layout(location=4) out vec3 ndcLastVS;

#ifdef VERTEX_COLORS
layout(location=5) out vec4 vertexColorsVS;
#endif

layout(set = 3, binding = 7) uniform sampler2D windNoiseMap;

layout(std430, set = 3, binding = 8) readonly buffer InstanceData {
    Instance instanceData[];
};

layout(push_constant) uniform constants {
    uint invertUVs;
    uint twoSided;
    uint materialIdx;
    float normalScale;
    float displacementScale;
    float windTextureLod;
    float windBendScale;
    float windWiggleScale;
} pushConstants;

void main() {

    Instance instance = instanceData[gl_InstanceIndex];
    texCoordVS = pushConstants.invertUVs > 0 ? vec2(vTexCoord.x, 1.0 - vTexCoord.y) : vTexCoord;
    
    mat4 mvMatrix = globalData.vMatrix;

    vec3 position = instance.position.xyz + vPosition;

    position = instance.position.xyz + WindAnimation(windNoiseMap, vPosition, pushConstants.windBendScale,
        pushConstants.windWiggleScale, pushConstants.windTextureLod,globalData.time, instance.position.xyz);
    vec3 lastPosition = instance.position.xyz + WindAnimation(windNoiseMap,  vPosition, pushConstants.windBendScale,
        pushConstants.windWiggleScale, pushConstants.windTextureLod,
        globalData.time - globalData.deltaTime, instance.position.xyz);

    vec4 positionToCamera = mvMatrix * vec4(position, 1.0);
#ifdef NORMAL_MAP
    positionVS = positionToCamera.xyz;
#endif
    
    gl_Position = globalData.pMatrix * positionToCamera;

    // Needed for velocity buffer calculation 
    ndcCurrentVS = vec3(gl_Position.xy, gl_Position.w);
    // For moving objects we need the last frames matrix
    vec4 last = globalData.pvMatrixLast * vec4(lastPosition, 1.0);
    ndcLastVS = vec3(last.xy, last.w);
    
    normalVS = mat3(mvMatrix) * vNormal;
    //normalVS *= -dot(normalVS, positionVS);

#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
    vec3 normal = normalize(normalVS);
    float correctionFactor = vTangent.w * (PushConstants.invertUVs > 0 ? -1.0 : 1.0);
    vec3 tangent = normalize(mat3(mvMatrix) * vTangent.xyz);
    
    vec3 bitangent = normalize(correctionFactor * 
        cross(tangent, normal));

    TBN = mat3(tangent, bitangent, normal);
#endif

#ifdef VERTEX_COLORS
    vertexColorsVS = vVertexColors;
#endif
    
}