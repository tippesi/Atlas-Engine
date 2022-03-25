#include <../common/utility.hsh>
#include <../common/PI.hsh>
#include <sharedUniforms.hsh>
#include <shoreInteraction.hsh>

layout(location=0)in vec3 vPosition;

layout(binding = 0) uniform sampler2D displacementMap;

out vec4 fClipSpace;
out vec3 fPosition;
out vec3 fModelCoord;
out vec3 fOriginalCoord;
out vec2 fTexCoord;
out float waterDepth;
out float shoreScaling;
out vec3 ndcCurrent;
out vec3 ndcLast;
out vec3 normalShoreWave;

uniform vec2 nodeLocation;
uniform float nodeSideLength;
uniform vec3 translation;

uniform mat4 vMatrix;
uniform mat4 pMatrix;

uniform float choppyScale;
uniform float displacementScale;
uniform float tiling;

uniform float leftLoD;
uniform float topLoD;
uniform float rightLoD;
uniform float bottomLoD;

uniform mat4 pvMatrixLast;

const float shoreStartScaling = 15.0;
const float shoreOffsetScaling = 5.0;
const float minShoreScaling = 0.3;

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
	
	waterDepth = shoreStartScaling;
	
	fPosition = stitch(vPosition) * nodeSideLength + 
		vec3(nodeLocation.x, 0.0, nodeLocation.y) + translation;
	
	bool hasTerrain = terrainSideLength > 0.0;

	vec2 terrainTex = (vec2(fPosition.xz) - vec2(terrainTranslation.xz))
		/ terrainSideLength;
		
	float shoreDistance = 0.0;
	vec2 shoreGradient = vec2(0.0);

	if (hasTerrain && terrainTex.x >= 0.0 && terrainTex.y >= 0.0
		&& terrainTex.x <= 1.0 && terrainTex.y <= 1.0) {
		waterDepth = fPosition.y - textureLod(terrainHeight, terrainTex, 0.0).r 
			* terrainHeightScale + terrainTranslation.y;
		shoreDistance = textureLod(terrainHeight, terrainTex, 0.0).g;
		shoreGradient = normalize(2.0 * textureLod(terrainHeight, terrainTex, 0.0).ba - 1.0);
	}
	
	float depthScaling = clamp((waterDepth - shoreOffsetScaling) / 
		(shoreStartScaling - shoreOffsetScaling), minShoreScaling, 1.0);
	shoreScaling = hasTerrain ? depthScaling : 1.0;
	
	vec2 vTexCoord = vec2(fPosition.x, fPosition.z) / tiling;

	fOriginalCoord = fPosition;
	
	fPosition.y += textureLod(displacementMap, vTexCoord, 0.0).r * displacementScale * shoreScaling;
	fPosition.x += textureLod(displacementMap, vTexCoord, 0.0).g * choppyScale * shoreScaling;
	fPosition.z += textureLod(displacementMap, vTexCoord, 0.0).b * choppyScale * shoreScaling;

	vec3 dx = vec3(0.1, 0.0, 0.0) + CalculateGerstner(fPosition + vec3(0.1, 0.0, 0.0));
	vec3 dz = vec3(0.0, 0.0, 0.1) + CalculateGerstner(fPosition + vec3(0.0, 0.0, 0.1));
	vec3 centerOffset = CalculateGerstner(fPosition);

	normalShoreWave = normalize(cross(dz - centerOffset, dx - centerOffset));
	fPosition += centerOffset;

	fModelCoord = fPosition;
	
	fPosition = vec3(vMatrix * vec4(fPosition, 1.0));
	fClipSpace = pMatrix * vec4(fPosition, 1.0);
	
	fTexCoord = vTexCoord;

	ndcCurrent = vec3(fClipSpace.xy, fClipSpace.w);
	// For moving objects we need the last matrix
	vec4 last = pvMatrixLast * vec4(fModelCoord, 1.0);
	ndcLast = vec3(last.xy, last.w);
	
	gl_Position = fClipSpace;
	
}