layout (location = 0) in vec2 vPosition;

uniform vec2 offset;
uniform float scale;

uniform mat4 mMatrix;

void main() {
	
	vec2 localPosition =  offset + scale * vPosition;
					
	gl_Position =  mMatrix * vec4(localPosition.x, 0.0f, localPosition.y, 1.0f);
	
}