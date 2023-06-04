#include <../common/utility.hsh>
#include <../common/PI.hsh>
#include <sharedUniforms.hsh>
#include <shoreInteraction.hsh>

layout(location=0) in vec3 vPosition;

layout(set = 3, binding = 0) uniform sampler2D displacementMap;

layout(location=0) out vec4 fClipSpace;
layout(location=1) out vec3 fPosition;
layout(location=2) out vec3 fModelCoord;
layout(location=3) out vec3 fOriginalCoord;
layout(location=4) out vec2 fTexCoord;
// layout(location=5) out float waterDepth;
layout(location=6) out float shoreScaling;
layout(location=7) out vec3 ndcCurrent;
layout(location=8) out vec3 ndcLast;
layout(location=9) out vec3 normalShoreWave;

const float shoreStartScaling = 15.0;
const float shoreOffsetScaling = 5.0;
const float minShoreScaling = 0.3;

vec3 stitch(vec3 position) {
	
	// Note: This only works because our grid has a constant size
	// which cannot be changed.
	position.xz *= 128.0;
	
	if (position.x == 0.0 && PushConstants.leftLoD > 1.0) {
		position.z = floor(position.z / PushConstants.leftLoD)
			* PushConstants.leftLoD;
	}
	else if (position.z == 0.0 && PushConstants.topLoD > 1.0) {
		position.x = floor(position.x / PushConstants.topLoD)
			* PushConstants.topLoD;
	}
	else if (position.x == 128.0 && PushConstants.rightLoD > 1.0) {
		position.z = floor(position.z / PushConstants.rightLoD)
			* PushConstants.rightLoD;
	}
	else if (position.z == 128.0 && PushConstants.bottomLoD > 1.0) {
		position.x = floor(position.x / PushConstants.bottomLoD)
			* PushConstants.bottomLoD;
	}
	
	position.xz /= 128.0;
	
	return position;
	
}

void main() {
	
	float waterDepth = shoreStartScaling;
	
	fPosition = stitch(vPosition) * PushConstants.nodeSideLength +
		vec3(PushConstants.nodeLocation.x, 0.0, PushConstants.nodeLocation.y)
		+ Uniforms.translation.xyz;
	
	bool hasTerrain = Uniforms.terrainSideLength > 0.0;

	vec2 terrainTex = (vec2(fPosition.xz) - vec2(Uniforms.terrainTranslation.xz))
		/ Uniforms.terrainSideLength;
		
	float shoreDistance = 0.0;
	vec2 shoreGradient = vec2(0.0);

	if (hasTerrain && terrainTex.x >= 0.0 && terrainTex.y >= 0.0
		&& terrainTex.x <= 1.0 && terrainTex.y <= 1.0) {
		waterDepth = fPosition.y - textureLod(terrainHeight, terrainTex, 0.0).r 
			* Uniforms.terrainHeightScale + Uniforms.terrainTranslation.y;
		shoreDistance = textureLod(terrainHeight, terrainTex, 0.0).g;
		shoreGradient = normalize(2.0 * textureLod(terrainHeight, terrainTex, 0.0).ba - 1.0);
	}
	
	float depthScaling = clamp((waterDepth - shoreOffsetScaling) / 
		(shoreStartScaling - shoreOffsetScaling), minShoreScaling, 1.0);
	shoreScaling = hasTerrain ? depthScaling : 1.0;
	
	vec2 vTexCoord = vec2(fPosition.x, fPosition.z) / Uniforms.tiling;

	fOriginalCoord = fPosition;
	
	fPosition.y += textureLod(displacementMap, vTexCoord, 0.0).r * Uniforms.displacementScale * shoreScaling;
	fPosition.x += textureLod(displacementMap, vTexCoord, 0.0).g * Uniforms.choppyScale * shoreScaling;
	fPosition.z += textureLod(displacementMap, vTexCoord, 0.0).b * Uniforms.choppyScale * shoreScaling;

	vec3 dx = vec3(0.1, 0.0, 0.0) + CalculateGerstner(fPosition + vec3(0.1, 0.0, 0.0));
	vec3 dz = vec3(0.0, 0.0, 0.1) + CalculateGerstner(fPosition + vec3(0.0, 0.0, 0.1));
	vec3 centerOffset = CalculateGerstner(fPosition);

	normalShoreWave = normalize(cross(dz - centerOffset, dx - centerOffset));
	fPosition += centerOffset;

	fModelCoord = fPosition;
	
	fPosition = vec3(globalData.vMatrix * vec4(fPosition, 1.0));
	fClipSpace = globalData.pMatrix * vec4(fPosition, 1.0);
	
	fTexCoord = vTexCoord;

	ndcCurrent = vec3(fClipSpace.xy, fClipSpace.w);
	// For moving objects we need the last matrix
	vec4 last = globalData.pvMatrixLast * vec4(fModelCoord, 1.0);
	ndcLast = vec3(last.xy, last.w);
	
	gl_Position = fClipSpace;
	
}