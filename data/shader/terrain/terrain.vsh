layout (location = 0) in vec2 vPosition;
layout (location = 1) in vec2 vPatchOffset;

uniform usampler2D heightField;

uniform float heightScale;

uniform float tileScale;
uniform vec2 patchOffsets[64];
uniform float patchOffsetsScale;

uniform vec2 nodeLocation;
uniform float nodeSideLength;

uniform mat4 mMatrix;

void main() {
	
	vec2 localPosition = vPatchOffset * patchOffsetsScale + tileScale * vPosition;
	vec2 position =  nodeLocation + localPosition;
	
	vec2 texCoords = localPosition;
	texCoords /= nodeSideLength;
	
	float height = float(texture(heightField, texCoords).r) / 65535.0f * heightScale;
					
	gl_Position =  mMatrix * vec4(position.x, height, position.y, 1.0f);
	
}