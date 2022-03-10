#include <ddgi.hsh>

// Per vertex attributes
layout(location=0) in vec3 vPosition;

#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
layout(location=3) in vec4 vTangent;
#endif

const float scale = 0.1;

// Vertex out parameters
out vec3 positionVS;
out vec3 normalVS;

out vec3 ndcCurrentVS;
out vec3 ndcLastVS;

out flat uint instanceID;
out vec3 worldSpaceNormal;

// Uniforms
uniform mat4 pMatrix;
uniform mat4 vMatrix;

uniform mat4 pvMatrixLast;

// Functions
void main() {

	instanceID = gl_InstanceID;

	ivec3 probeCoord = GetProbeGridCoord(gl_InstanceID);
	vec3 probeOffset = GetProbeOffset(gl_InstanceID);
	vec3 probePosition = GetProbePosition(probeCoord);
	
	mat4 mvMatrix = vMatrix;

	// Move any animation code to their own compute shaders
	vec3 position = probeOffset + probePosition + vPosition * scale;
	vec4 positionToCamera = mvMatrix * vec4(position, 1.0);
	positionVS = positionToCamera.xyz;
	
	gl_Position = pMatrix * positionToCamera;

	// Needed for velocity buffer calculation 
	ndcCurrentVS = vec3(gl_Position.xy, gl_Position.w);
	// For moving objects we need the last frames matrix
	vec3 lastPosition = position;
	vec4 last = pvMatrixLast * vec4(lastPosition, 1.0);
	ndcLastVS = vec3(last.xy, last.w);
	
	worldSpaceNormal = normalize(vPosition);
	normalVS = mat3(mvMatrix) * normalize(vPosition);
	
}