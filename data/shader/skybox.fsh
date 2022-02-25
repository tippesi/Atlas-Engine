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
	
	colorFS = textureLod(skyCubemap, texCoordVS, 0).xyz;
	
	 // Calculate screen space velocity
	vec2 ndcL = ndcLastVS.xy / ndcLastVS.z;
	vec2 ndcC = ndcCurrentVS.xy / ndcCurrentVS.z;

	ndcL -= jitterLast;
	ndcC -= jitterCurrent;

	velocityFS = (ndcL - ndcC) * 0.5;
	
}