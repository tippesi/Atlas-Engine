layout (location = 0) in vec2 vPosition;
layout (location = 1) in vec2 vPatchOffset;

layout(binding = 0) uniform usampler2D heightField;
layout(binding = 2) uniform usampler2D splatMap;

flat out uvec4 materialIndicesVS;

uniform float heightScale;

uniform float tileScale;
uniform float patchSize;

uniform vec2 nodeLocation;
uniform float nodeSideLength;
uniform float normalTexelSize;

uniform float leftLoD;
uniform float topLoD;
uniform float rightLoD;
uniform float bottomLoD;

vec2 stitch(vec2 position) {
	
	// We have 8x8 patches per node
	float nodeSize = 8.0 * patchSize;
	
	if (position.x == 0.0 && leftLoD > 1.0) {
		position.y = floor(position.y / leftLoD) * leftLoD;
	}
	else if (position.y == 0.0 && topLoD > 1.0) {
		position.x = floor(position.x / topLoD) * topLoD;
	}
	else if (position.x == nodeSize && rightLoD > 1.0) {
		position.y = floor(position.y / rightLoD) * rightLoD;
	}
	else if (position.y == nodeSize && bottomLoD > 1.0) {
		position.x = floor(position.x / bottomLoD) * bottomLoD;
	}
	
	return position;
	
}

void main() {
	
	vec2 localPosition = vPatchOffset * patchSize + vPosition;
	
	localPosition = stitch(localPosition) * tileScale;
	
	vec2 position =  nodeLocation + localPosition;
	
	vec2 texCoords = localPosition;
	texCoords /= nodeSideLength;
	
	float texel = 1.0 / (8.0 * patchSize);
	
	materialIndicesVS.x = texture(splatMap, texCoords).r;
	materialIndicesVS.y = texture(splatMap, texCoords + vec2(texel, 0.0)).r;
	materialIndicesVS.z = texture(splatMap, texCoords + vec2(0.0, texel)).r;
	materialIndicesVS.w = texture(splatMap, texCoords + vec2(texel, texel)).r;	
	
	// The middle of the texel should match the vertex position
	float height = float(texture(heightField, texCoords).r) / 65535.0 * heightScale;
					
	gl_Position =  vec4(position.x, height, position.y, 1.0);
	
}