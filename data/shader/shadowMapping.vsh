#include <globals.hsh>
#include <wind.hsh>

layout(location=0) in vec3 vPosition;
#ifdef OPACITY_MAP
layout(location=2) in vec2 vTexCoord;
#endif
#ifdef SKINNED
layout(location=5) in uvec4 vJoints;
layout(location=6) in vec4 vWeights;
#endif

#ifdef OPACITY_MAP
layout(location=0) out vec2 texCoordVS;
#endif

layout(std430, set = 1, binding = 1) readonly buffer Matrices {
    mat3x4 matrices[];
};

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
} PushConstants;

void main() {

    mat4 mMatrix = mat4(transpose(matrices[gl_InstanceIndex]));
    
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

    if (PushConstants.vegetation > 0) {

        // This is not well optimized and could use some work
        vec3 windPosition = vec3(mMatrix * vec4(position, 1.0));
        vec2 windDir = normalize((inverse(mMatrix) * vec4(globalData.windDir.x, 0.0, globalData.windDir.y, 0.0)).xz);
        position = WindAnimation(windNoiseMap, position, windDir, PushConstants.windBendScale,
            PushConstants.windWiggleScale, PushConstants.windTextureLod, globalData.time, windPosition);

    }
    
    gl_Position = PushConstants.lightSpaceMatrix * matrix * vec4(position, 1.0f);
    
}