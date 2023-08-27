#include <../wind.hsh>
#include <../globals.hsh>

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

layout(std430, set = 1, binding = 0) buffer CurrentMatrices {
    mat3x4 currentMatrices[];
};

layout(std430, set = 1, binding = 1) buffer LastMatrices {
    mat3x4 lastMatrices[];
};

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
layout(location=6) out mat3 TBN;
#endif

layout(push_constant) uniform constants {
    uint vegetation;
    uint invertUVs;
    uint twoSided;
    uint staticMesh;
    uint materialIdx;
    float normalScale;
    float displacementScale;
} PushConstants;

// Functions
void main() {

    mat4 mMatrix = mat4(transpose(currentMatrices[gl_InstanceIndex]));
    mat4 mMatrixLast = PushConstants.staticMesh > 0 ? mMatrix : mat4(transpose(lastMatrices[gl_InstanceIndex]));

#ifdef TEX_COORDS
    texCoordVS = PushConstants.invertUVs > 0 ? vec2(vTexCoord.x, 1.0 - vTexCoord.y) : vTexCoord;
#endif
    
    mat4 mvMatrix = globalData.vMatrix * mMatrix;

    vec3 position = vPosition;
    vec3 lastPosition = vPosition;

    if (PushConstants.vegetation > 0) {

        position = WindAnimation(vPosition, globalData.time, mMatrix[3].xyz);
        lastPosition = WindAnimation(vPosition, globalData.time - globalData.deltaTime, mMatrix[3].xyz);

    }

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
    
#ifdef GENERATE_IMPOSTOR
    mvMatrix = globalData.mMatrix;
#endif
    
    normalVS = mat3(mvMatrix) * vNormal;    

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