layout (location = 0) out vec3 fragColor;
layout (location = 1) out vec2 velocity;

layout(binding = 0) uniform samplerCube skyCubemap;

in vec3 fTexCoord;
in vec3 ndcCurrent;
in vec3 ndcLast;

uniform vec2 jitterLast;
uniform vec2 jitterCurrent;

void main() {
	
	fragColor = texture(skyCubemap, fTexCoord).xyz;
	
	 // Calculate velocity
	vec2 ndcL = ndcLast.xy / ndcLast.z;
	vec2 ndcC = ndcCurrent.xy / ndcCurrent.z;

	ndcL -= jitterLast;
	ndcC -= jitterCurrent;

	velocity = (ndcL - ndcC) * 0.5;
	
}