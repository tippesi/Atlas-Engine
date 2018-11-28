layout (location = 0) in vec2 vPosition;

uniform float scale;
uniform vec2 patchOffsets[64];
uniform float patchOffsetsScale;
uniform vec2 nodeLocation;

uniform mat4 mMatrix;

void main() {
	
	vec2 localPosition =  nodeLocation + patchOffsets[gl_InstanceID] * patchOffsetsScale + scale * vPosition;
					
	gl_Position =  mMatrix * vec4(localPosition.x, 0.0f, localPosition.y, 1.0f);
	
}