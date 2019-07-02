layout(location=0)in vec3 vPosition;

uniform mat4 vMatrix;
uniform mat4 pMatrix;
uniform mat4 mMatrix;

void main() {
	
	gl_Position = pMatrix * vMatrix * mMatrix * vec4(vPosition, 1.0f); 
	
}