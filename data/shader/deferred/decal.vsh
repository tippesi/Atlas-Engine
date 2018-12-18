layout(location=0)in vec3 vPosition;

out vec3 fTexCoordProj;
out vec3 viewSpacePosition;
out vec2 fTexCoord;

uniform mat4 mMatrix;
uniform mat4 vMatrix;
uniform mat4 pMatrix;

void main() {

    fTexCoord = vPosition.xz;
	
    vec4 position = vMatrix * mMatrix * vec4(vPosition, 1.0);

	viewSpacePosition = position.xyz;
	gl_Position = pMatrix * position;
	
	fTexCoordProj = gl_Position.xyw;
	
}