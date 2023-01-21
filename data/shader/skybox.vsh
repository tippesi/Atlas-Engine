#include <globals.hsh>

layout(location = 0) in vec3 vPosition;

// layout (location = 0) out vec3 worldPositionVS;
layout (location = 1) out vec3 texCoordVS;
layout (location = 2) out vec3 ndcCurrentVS;
layout (location = 3) out vec3 ndcLastVS;

layout(push_constant) uniform constants {
	vec4 cameraLocationLast;
} PushConstants;

void main() {
	
    vec4 pos = globalData.pMatrix * globalData.vMatrix * vec4(vPosition + globalData.cameraLocation.xyz, 1.0);
    gl_Position = pos.xyww;
	
	// We need position for fog calculation
	// worldPositionVS = vec3(vPosition);
	
	// Velocity buffer
	ndcCurrentVS = vec3(gl_Position.xy, gl_Position.w);
	// For moving objects we need the last matrix
	vec4 last = (globalData.pvMatrixLast * vec4(vPosition + PushConstants.cameraLocationLast.xyz, 1.0)).xyww;
	ndcLastVS = vec3(last.xy, last.w);
	
    texCoordVS = vPosition;
	
}  