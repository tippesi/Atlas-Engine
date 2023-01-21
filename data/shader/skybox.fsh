#include <globals.hsh>

layout (location = 0) out vec4 colorFS;
layout (location = 1) out vec2 velocityFS;

layout(set = 3, binding = 0) uniform samplerCube skyCubemap;

// layout (location = 0) in vec3 worldPositionVS;
layout (location = 1) in vec3 texCoordVS;
layout (location = 2) in vec3 ndcCurrentVS;
layout (location = 3) in vec3 ndcLastVS;

void main() {
	
	colorFS = vec4(textureLod(skyCubemap, texCoordVS, 0).xyz, 1.0);
	
	 // Calculate screen space velocity
	vec2 ndcL = ndcLastVS.xy;
	vec2 ndcC = ndcCurrentVS.xy;

	ndcL -= globalData.jitterLast;
	ndcC -= globalData.jitterCurrent;

	velocityFS = (ndcL - ndcC) * 0.5;
	
}