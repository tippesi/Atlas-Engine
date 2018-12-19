layout(location=0)in vec3 vPosition;

out vec3 fTexCoordProj;
out mat4 inverseModelMatrix;

uniform mat4 mMatrix;
uniform mat4 vMatrix;
uniform mat4 pMatrix;

void main() {
	
	// Faster on the GPU
	inverseModelMatrix = inverse(mMatrix);
	
    vec4 position = vMatrix * mMatrix * vec4(vPosition, 1.0);

	gl_Position = pMatrix * position;
	
	fTexCoordProj = gl_Position.xyw;
	
}