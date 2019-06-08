layout(location=0)in vec3 vPosition;

layout(binding = 0) uniform sampler2D displacementMap;

out vec4 fClipSpace;
out vec3 fPosition;
out vec3 fModelCoord;
out vec2 fTexCoord;

uniform vec2 nodeLocation;
uniform float nodeSideLength;
uniform float oceanHeight;

uniform mat4 vMatrix;
uniform mat4 pMatrix;

uniform float choppyScale;
uniform float displacementScale;
uniform float tiling;

uniform float leftLoD;
uniform float topLoD;
uniform float rightLoD;
uniform float bottomLoD;

vec3 stitch(vec3 position) {
	
	// Note: This only works because our grid has a constant size
	// which cannot be changed.
	position.xz *= 128.0;
	
	if (position.x == 0.0 && leftLoD > 1.0) {
		position.z = floor(position.z / leftLoD) * leftLoD;
	}
	else if (position.z == 0.0 && topLoD > 1.0) {
		position.x = floor(position.x / topLoD) * topLoD;
	}
	else if (position.x == 128.0 && rightLoD > 1.0) {
		position.z = floor(position.z / rightLoD) * rightLoD;
	}
	else if (position.z == 128.0 && bottomLoD > 1.0) {
		position.x = floor(position.x / bottomLoD) * bottomLoD;
	}
	
	position.xz /= 128.0;
	
	return position;
	
}

void main() {
	
	fPosition = stitch(vPosition) * nodeSideLength + 
		vec3(nodeLocation.x, oceanHeight, nodeLocation.y);
	
	vec2 vTexCoord = vec2(fPosition.x, fPosition.z) / tiling;
	
	fPosition.y += texture(displacementMap, vTexCoord).r * displacementScale;
	fPosition.x += texture(displacementMap, vTexCoord).g * choppyScale;
	fPosition.z += texture(displacementMap, vTexCoord).b * choppyScale;
	
	fModelCoord = fPosition;
	
	fClipSpace = pMatrix * vMatrix * vec4(fPosition, 1.0);
	
	fTexCoord = vec2(fPosition.x, fPosition.z) / tiling;
	fPosition = vec3(vMatrix * vec4(fPosition, 1.0));
	
	gl_Position = fClipSpace;
	
}