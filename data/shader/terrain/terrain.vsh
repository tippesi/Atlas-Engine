#include <../globals.hsh>

#ifndef DISTANCE
layout (location = 0) in vec2 vPosition;
#else
layout (location = 0) in vec3 vPosition;
#endif

layout(set = 3, binding = 0) uniform usampler2D heightField;
layout(set = 3, binding = 2) uniform usampler2D splatMap;

#ifdef DISTANCE
layout(location=0) out vec2 materialTexCoords;
layout(location=1) out vec2 texCoords;
layout(location=2) out vec3 ndcCurrent;
layout(location=3) out vec3 ndcLast;
#endif

layout (set = 3, binding = 9, std140) uniform UniformBuffer {
    vec4 frustumPlanes[6];

    float heightScale;
    float displacementDistance;

    float tessellationFactor;
    float tessellationSlope;
    float tessellationShift;
    float maxTessellationLevel;
} Uniforms;

layout(push_constant) uniform constants {
    float nodeSideLength;
    float tileScale;
    float patchSize;
    float normalTexelSize;

    float leftLoD;
    float topLoD;
    float rightLoD;
    float bottomLoD;

    vec2 nodeLocation;
} PushConstants;

vec2 stitch(vec2 position) {
    
    // We have 8x8 patches per node
    float nodeSize = 8.0 * PushConstants.patchSize;
    
    if (position.x == 0.0 && PushConstants.leftLoD > 1.0) {
        position.y = floor(position.y / PushConstants.leftLoD)
            * PushConstants.leftLoD;
    }
    else if (position.y == 0.0 && PushConstants.topLoD > 1.0) {
        position.x = floor(position.x / PushConstants.topLoD)
            * PushConstants.topLoD;
    }
    else if (position.x == nodeSize && PushConstants.rightLoD > 1.0) {
        position.y = floor(position.y / PushConstants.rightLoD)
            * PushConstants.rightLoD;
    }
    else if (position.y == nodeSize && PushConstants.bottomLoD > 1.0) {
        position.x = floor(position.x / PushConstants.bottomLoD)
            * PushConstants.bottomLoD;
    }
    
    return position;
    
}

void main() {

#ifndef DISTANCE
    vec2 patchOffset = vec2(gl_InstanceIndex / 8, gl_InstanceIndex % 8);
    vec2 localPosition = patchOffset * PushConstants.patchSize + vPosition;
#else
    vec2 localPosition = vPosition.xz;
#endif

    localPosition = stitch(localPosition) * PushConstants.tileScale;
    
    vec2 position = PushConstants.nodeLocation + localPosition;

#ifndef DISTANCE
    vec2 texCoords = localPosition;
#else
    texCoords = localPosition;
    materialTexCoords = texCoords;
#endif

    texCoords /= PushConstants.nodeSideLength;

    // The middle of the texel should match the vertex position
    float height = float(texture(heightField, texCoords).r) / 65535.0 * Uniforms.heightScale;

    vec4 worldPosition = vec4(position.x, height, position.y, 1.0);

#ifndef DISTANCE
    gl_Position =  worldPosition;
#else
    gl_Position =  globalData.pMatrix * globalData.vMatrix * worldPosition;

    ndcCurrent = vec3(gl_Position.xy, gl_Position.w);

    vec4 last = globalData.pvMatrixLast * worldPosition;
    ndcLast = vec3(last.xy, last.w);
#endif
    
}