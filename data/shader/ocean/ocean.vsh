layout(location=0)in vec3 vPosition;

layout(binding = 0) uniform sampler2D displacementMap;

out vec4 fClipSpace;
out vec3 fPosition;
out vec3 fModelCoord;
out vec2 fTexCoord;

uniform vec2 nodeLocation;
uniform float nodeSideLength;
uniform float nodeHeight;

uniform mat4 vMatrix;
uniform mat4 pMatrix;

uniform float choppyScale;
uniform float displacementScale;

void main() {
	
	fPosition = vPosition * nodeSideLength + 
		vec3(nodeLocation.x, nodeHeight, nodeLocation.y);
	
	vec2 vTexCoord = vec2(fPosition.x, fPosition.z) / 32.0f;
	
	fPosition.y = texture(displacementMap, vTexCoord).r * displacementScale;
	fPosition.x += texture(displacementMap, vTexCoord).g * choppyScale;
	fPosition.z += texture(displacementMap, vTexCoord).b * choppyScale;
	
	fModelCoord = fPosition;
	
	fClipSpace = pMatrix * vMatrix * vec4(fPosition, 1.0);
	
	fTexCoord = vec2(fPosition.x, fPosition.z) / 32.0f;
	fPosition = vec3(vMatrix * vec4(fPosition, 1.0));
	
	gl_Position = fClipSpace;
	
}