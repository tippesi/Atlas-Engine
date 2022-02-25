#include <wind.hsh>

layout(location=0)in vec3 vPosition;
#ifdef OPACITY_MAP
layout(location=2)in vec2 vTexCoord;
#endif

layout(std430, binding = 2) buffer Matrices {
	mat4 matrices[];
};


uniform mat4 lightSpaceMatrix;

uniform float time;

uniform bool vegetation;
uniform bool invertUVs;

#ifdef OPACITY_MAP
out vec2 texCoordVS;
#endif

void main() {

	mat4 mMatrix = matrices[gl_InstanceID];
	
#ifdef OPACITY_MAP
	texCoordVS = invertUVs ? vec2(vTexCoord.x, 1.0 - vTexCoord.y) : vTexCoord;
#endif

	mat4 matrix = mMatrix;

	vec3 position = vPosition;

	if (vegetation) {

		position = WindAnimation(vPosition, time, mMatrix[3].xyz);

	}
	
	gl_Position = lightSpaceMatrix * matrix * vec4(position, 1.0f); 
	
}