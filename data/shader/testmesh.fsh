layout (location = 0) out vec4 colorFS;

layout (location = 0) in vec3 positionVS;
layout (location = 1) in vec3 normalVS;

//push constants block
layout(push_constant) uniform constants {
	mat4 vMatrix;
	mat4 pMatrix;
} PushConstants;

const vec3 lightDirection = -vec3(1.0, 1.0, 0.0);
const vec3 lightColor = vec3(1.0);
const float lightAmbient = 0.1;

void main() {

	float NdotL = dot(normalize(normalVS), -normalize(lightDirection));
	colorFS = vec4(NdotL * lightColor + lightAmbient * lightColor, 1.0);

}