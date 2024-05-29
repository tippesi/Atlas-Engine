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
layout(location=1) in vec4 vNormal;
#ifdef TEX_COORDS
layout(location=2) in vec2 vTexCoord;
#endif

#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
layout(location=3) in vec4 vTangent;
#endif

#ifdef VERTEX_COLORS
layout(location=4) in vec4 vVertexColors;
#endif

layout(std430, set = 1, binding = 1) buffer CurrentMatrices {
    mat3x4 currentMatrices[];
};

layout(std430, set = 1, binding = 2) buffer LastMatrices {
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

layout(location=6) out float normalInversionVS;

// Matrix is several locations
#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
layout(location=7) out mat3 TBN;
#endif

layout(set = 3, binding = 7) uniform sampler2D windNoiseMap;

layout(push_constant) uniform constants {
    uint vegetation;
    uint invertUVs;
    uint twoSided;
    uint staticMesh;
    uint materialIdx;
    float normalScale;
    float displacementScale;
    float windTextureLod;
    float windBendScale;
    float windWiggleScale;
} PushConstants;

// Functions
void main() {

    mat4 mMatrix = mat4(transpose(currentMatrices[gl_InstanceIndex]));
    mat4 mMatrixLast = PushConstants.staticMesh > 0 ? mMatrix : mat4(transpose(lastMatrices[gl_InstanceIndex]));

    normalInversionVS = determinant(mMatrix) > 0.0 ? 1.0 : -1.0;

#ifdef TEX_COORDS
    texCoordVS = PushConstants.invertUVs > 0 ? vec2(vTexCoord.x, 1.0 - vTexCoord.y) : vTexCoord;
#endif
    
    mat4 mvMatrix = globalData.vMatrix * mMatrix;

    vec3 position = vPosition;
    vec3 lastPosition = vPosition;

    if (PushConstants.vegetation > 0) {

        // This is not well optimized and could use some work
        vec3 windPosition = vec3(mMatrix * vec4(position, 1.0));
        vec2 windDir = normalize((inverse(mMatrix) * vec4(globalData.windDir.x, 0.0, globalData.windDir.y, 0.0)).xz);
        position = WindAnimation(windNoiseMap, vPosition, windDir, PushConstants.windBendScale,
            PushConstants.windWiggleScale, PushConstants.windTextureLod, globalData.time, windPosition);
        lastPosition = WindAnimation(windNoiseMap, vPosition, windDir, PushConstants.windBendScale,
            PushConstants.windWiggleScale, PushConstants.windTextureLod,
            globalData.time - globalData.deltaTime, windPosition);

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
    
    normalVS = mat3(mvMatrix) * vNormal.xyz;

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