layout(location=0)in vec2 vPosition;
layout(location=1)in vec4 position;

out vec2 fTexCoord;
out float blend;
out float depth;

uniform mat4 pMatrix;
uniform mat4 vMatrix;

uniform vec3 quadVector1;
uniform vec3 quadVector2;

uniform float size;


void main() {
	
	fTexCoord = (vPosition + 1.0) / 2.0;
	
	vec4 viewPosition = vMatrix * vec4(position.xyz + (quadVector1 * vPosition.y + quadVector2 * vPosition.x) * size, 1.0f);
	
	depth = viewPosition.z;
	
	blend = position.w;
	
	gl_Position =  pMatrix * viewPosition;
   
}