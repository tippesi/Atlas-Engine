#include <../wind.hsh>
#include <buffers.hsh>

// Per vertex attributes
layout(location=0) in vec3 vPosition;
layout(location=2) in vec2 vTexCoord;

// Vertex out parameters
out vec3 positionVS;
out vec2 texCoordVS;

// Uniforms
uniform mat4 pMatrix;
uniform mat4 vMatrix;

uniform float time;
uniform float deltaTime;

uniform bool invertUVs;

// Functions
void main() {

    Instance instance = instanceData[gl_InstanceID + gl_BaseInstance];
    texCoordVS = invertUVs ? vec2(vTexCoord.x, 1.0 - vTexCoord.y) : vTexCoord;
    
    mat4 mvMatrix = vMatrix;

    vec3 position = instance.position.xyz + vPosition;
    position = instance.position.xyz + WindAnimation(vPosition, time, instance.position.xyz);

    vec4 positionToCamera = mvMatrix * vec4(position, 1.0);
    positionVS = positionToCamera.xyz;
    
    gl_Position = pMatrix * positionToCamera;
    
}