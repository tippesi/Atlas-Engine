layout (location = 0) in vec2 vPosition;

uniform sampler2D heightField;

uniform float heightScale;

uniform float scale;
uniform vec2 patchOffsets[64];
uniform float patchOffsetsScale;

uniform vec2 nodeLocation;
uniform float nodeSideLength;

uniform mat4 mMatrix;

void main() {
	
	vec2 localPosition = patchOffsets[gl_InstanceID] * patchOffsetsScale + scale * vPosition;
	vec2 position =  nodeLocation + localPosition;
	
	vec2 texCoords = localPosition;
	texCoords /= nodeSideLength;
	
	float height = texture(heightField, texCoords).r * heightScale;
					
	gl_Position =  mMatrix * vec4(position.x, height, position.y, 1.0f);
	
}