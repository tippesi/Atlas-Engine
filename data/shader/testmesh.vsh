layout(location=0) in vec3 vPosition;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec2 vTexCoord;
layout(location=3) in vec4 vTangent;

layout (location = 0) out vec3 positionVS;
layout (location = 1) out vec3 normalVS;

//push constants block
layout(push_constant) uniform constants {
	mat4 vMatrix;
	mat4 pMatrix;
} PushConstants;

layout(set = 0, binding = 0) uniform  CameraBuffer{
	mat4 vMatrix;
	mat4 pMatrix;
} cameraData;

void main() {

	positionVS = vec3(cameraData.pMatrix * cameraData.vMatrix * vec4(vPosition, 1.0));
	normalVS = vNormal;

	//output the position of each vertex
	gl_Position = cameraData.pMatrix * cameraData.vMatrix * vec4(vPosition, 1.0);

}