#include "../structures"

layout(location=0)in vec3 vPosition;

out vec2 fTexCoord;

uniform mat4 vpMatrix;
uniform Light light;

void main() {
	
    gl_Position = vpMatrix * vec4(vPosition * light.radius + light.location, 1.0);
	
	fTexCoord = ((gl_Position.xy / gl_Position.w) + 1.0f) / 2.0f;
	
}