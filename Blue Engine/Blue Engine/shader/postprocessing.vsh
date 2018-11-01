layout(location=0)in vec2 vPosition;

out vec2 fPosition;

void main() {
	
	fPosition = vPosition;
	gl_Position = vec4(vPosition, 0.0, 1.0);
	
}