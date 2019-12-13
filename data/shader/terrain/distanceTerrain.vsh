layout (location = 0) in vec3 vPosition;

uniform usampler2D heightField;
uniform sampler2D splatMap;

out vec2 texCoords;
out vec2 materialTexCoords;
out vec4 splat;
out vec3 ndcCurrent;
out vec3 ndcLast;

uniform float heightScale;

uniform float tileScale;
uniform float patchSize;

uniform vec2 nodeLocation;
uniform float nodeSideLength;

uniform float leftLoD;
uniform float topLoD;
uniform float rightLoD;
uniform float bottomLoD;

uniform mat4 vMatrix;
uniform mat4 pMatrix;

uniform mat4 pvMatrixLast;

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
	
	vec2 localPosition = vPosition.xz;
	
	localPosition = stitch(localPosition) * tileScale;
	
	vec2 position =  nodeLocation + localPosition;
	
	materialTexCoords = localPosition;
	texCoords = materialTexCoords / nodeSideLength;
	splat = texture(splatMap, texCoords);
	
	// The middle of the texel should match the vertex position
	float height = float(texture(heightField, texCoords).r) / 65535.0 * heightScale;	
					
	gl_Position =  pMatrix * vMatrix * vec4(position.x, height, position.y, 1.0);
	
	// Velocity buffer
	ndcCurrent = vec3(gl_Position.xy, gl_Position.w);
	
	vec4 last = pvMatrixLast * vec4(position.x, height, position.y, 1.0);
	ndcLast = vec3(last.xy, last.w);
	
}