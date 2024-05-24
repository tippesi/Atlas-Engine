#include <../globals.hsh>
#include <ddgi.hsh>

// Per vertex attributes
layout(location=0) in vec3 vPosition;

const float scale = 0.1;

// Vertex out parameters
layout(location=0) out vec3 normalVS;

layout(location=1) out vec3 ndcCurrentVS;
layout(location=2) out vec3 ndcLastVS;

layout(location=3) out flat uint instanceID;
layout(location=4) out vec3 worldSpaceNormal;

// Functions
void main() {

    instanceID = gl_InstanceIndex;

    int probeCascadeIndex = int(gl_InstanceIndex) / 1000;

    probeCascadeIndex = 7;
    ivec3 probeCoord = GetProbeGridCoord(instanceID);
    vec3 probeOffset = GetProbeOffset(instanceID);
    vec3 probePosition = GetProbePosition(probeCoord, probeCascadeIndex);
    
    mat4 mvMatrix = globalData.vMatrix;

    // Move any animation code to their own compute shaders
    vec3 position = probeOffset + probePosition + vPosition * scale;
    vec4 positionToCamera = mvMatrix * vec4(position, 1.0);
    
    gl_Position = globalData.pMatrix * positionToCamera;

    // Needed for velocity buffer calculation 
    ndcCurrentVS = vec3(gl_Position.xy, gl_Position.w);
    // For moving objects we need the last frames matrix
    vec3 lastPosition = position;
    vec4 last = globalData.pvMatrixLast * vec4(lastPosition, 1.0);
    ndcLastVS = vec3(last.xy, last.w);
    
    worldSpaceNormal = normalize(vPosition);
    normalVS = mat3(mvMatrix) * normalize(vPosition);
    
}