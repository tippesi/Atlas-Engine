layout(location=0)in vec2 vPosition;

out vec2 fTexCoord;

void main() {
	
	fTexCoord = (vPosition + 1.0) / 2.0;
	gl_Position = vec4(vPosition, 0.0, 1.0);
   
}