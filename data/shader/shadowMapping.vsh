#include <globals.hsh>
#include <wind.hsh>

layout(location=0) in vec3 vPosition;
#ifdef OPACITY_MAP
layout(location=2) in vec2 vTexCoord;
#endif

#ifdef OPACITY_MAP
layout(location=0) out vec2 texCoordVS;
#endif

layout(std430, set = 1, binding = 0) buffer Matrices {
	mat4 matrices[];
};

layout(push_constant) uniform constants {
	mat4 lightSpaceMatrix;
	uint vegetation;
	uint invertUVs;
} PushConstants;

void main() {

	mat4 mMatrix = matrices[gl_InstanceIndex];
	
#ifdef OPACITY_MAP
	texCoordVS = PushConstants.invertUVs > 0 ? vec2(vTexCoord.x, 1.0 - vTexCoord.y) : vTexCoord;
#endif

	mat4 matrix = mMatrix;
	vec3 position = vPosition;

	if (PushConstants.vegetation > 0) {

		position = WindAnimation(vPosition, globalData.time, mMatrix[3].xyz);

	}

	mat4 clip = mat4(1.0f, 0.0f, 0.0f, 0.0f,
		0.0f,-1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.5f, 0.0f,
		0.0f, 0.0f, 0.5f, 1.0f);
	
	gl_Position = clip * PushConstants.lightSpaceMatrix * matrix * vec4(position, 1.0f);
	
}