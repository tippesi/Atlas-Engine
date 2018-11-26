#include "../structures"

layout(location=0)in vec3 vPosition;

out vec3 fTexCoordProj;

uniform mat4 vpMatrix;
uniform Light light;

void main() {
	
    gl_Position = vpMatrix * vec4(vPosition * light.radius + light.location, 1.0);

	fTexCoordProj = gl_Position.xyw;
	
}