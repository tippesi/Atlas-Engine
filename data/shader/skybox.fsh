#include <fog>

layout (location = 0) out vec3 colorFS;
layout (location = 1) out vec2 velocityFS;

layout(binding = 0) uniform samplerCube skyCubemap;

in vec3 worldPositionVS;
in vec3 texCoordVS;
in vec3 ndcCurrentVS;
in vec3 ndcLastVS;

uniform vec3 cameraLocation;
uniform vec3 lightDirection;
uniform vec3 lightColor;

uniform vec2 jitterLast;
uniform vec2 jitterCurrent;

void main() {
	
	colorFS = texture(skyCubemap, texCoordVS).xyz;
	
	const float far = 1000.0;
	colorFS = applyFog(colorFS, far, 
		cameraLocation, normalize(texCoordVS),
		-lightDirection, lightColor);
	
	 // Calculate screen space velocity
	vec2 ndcL = ndcLastVS.xy / ndcLastVS.z;
	vec2 ndcC = ndcCurrentVS.xy / ndcCurrentVS.z;

	ndcL -= jitterLast;
	ndcC -= jitterCurrent;

	velocityFS = (ndcL - ndcC) * 0.5;
	
}