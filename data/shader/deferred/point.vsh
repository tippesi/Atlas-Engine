#include "../structures"

layout(location=0)in vec3 vPosition;

out vec3 fTexCoordProj;
out vec3 viewSpacePosition;

uniform mat4 vMatrix;
uniform mat4 pMatrix;
uniform Light light;

void main() {
	
    vec4 position = vMatrix * vec4(vPosition * light.radius + light.location, 1.0);

	viewSpacePosition = position.xyz;
	gl_Position = pMatrix * position;
	
	fTexCoordProj = gl_Position.xyw;
	
}