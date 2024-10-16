#include <globals.hsh>
#include <wind.hsh>

layout(location=0) in vec3 vPosition;
#ifdef OPACITY_MAP
layout(location=2) in vec2 vTexCoord;
#endif

#ifdef OPACITY_MAP
layout(location=0) out vec2 texCoordVS;
#endif

layout(std430, set = 1, binding = 1) buffer Matrices {
    mat3x4 matrices[];
};

layout(set = 3, binding = 0) uniform sampler2D windNoiseMap;

layout(push_constant) uniform constants {
    mat4 lightSpaceMatrix;
    uint vegetation;
    uint invertUVs;
    float windTextureLod;
    float windBendScale;
    float windWiggleScale;
    uint textureID;
} PushConstants;

void main() {

    mat4 mMatrix = mat4(transpose(matrices[gl_InstanceIndex]));
    
#ifdef OPACITY_MAP
    texCoordVS = PushConstants.invertUVs > 0 ? vec2(vTexCoord.x, 1.0 - vTexCoord.y) : vTexCoord;
#endif

    mat4 matrix = mMatrix;
    vec3 position = vPosition;

    if (PushConstants.vegetation > 0) {

        // This is not well optimized and could use some work
        vec3 windPosition = vec3(mMatrix * vec4(position, 1.0));
        vec2 windDir = normalize((inverse(mMatrix) * vec4(globalData.windDir.x, 0.0, globalData.windDir.y, 0.0)).xz);
        position = WindAnimation(windNoiseMap, vPosition, windDir, PushConstants.windBendScale,
            PushConstants.windWiggleScale, PushConstants.windTextureLod, globalData.time, windPosition);

    }
    
    gl_Position = PushConstants.lightSpaceMatrix * matrix * vec4(position, 1.0f);
    
}