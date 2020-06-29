#include <../common/utility.hsh>
#include <../common/PI.hsh>

layout(location=0)in vec3 vPosition;

layout(binding = 0) uniform sampler2D displacementMap;
layout(binding = 9) uniform sampler2D terrainHeight;

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
out float foamShoreWave;
out float breakingShoreWave;

uniform vec2 nodeLocation;
uniform float nodeSideLength;
uniform vec3 translation;

uniform mat4 vMatrix;
uniform mat4 pMatrix;
uniform vec3 cameraLocation;

uniform float choppyScale;
uniform float displacementScale;
uniform float tiling;

uniform float shoreWaveDistanceOffset;
uniform float shoreWaveDistanceScale;
uniform float shoreWaveAmplitude;
uniform float shoreWaveSteepness;
uniform float shoreWavePower;
uniform float shoreWaveSpeed;
uniform float shoreWaveLength;

uniform float leftLoD;
uniform float topLoD;
uniform float rightLoD;
uniform float bottomLoD;

uniform vec3 terrainTranslation;
uniform float terrainSideLength;
uniform float terrainHeightScale;

uniform float time;

uniform mat4 pvMatrixLast;

const float shoreStartScaling = 15.0;
const float shoreOffsetScaling = 5.0;
const float minShoreScaling = 0.0;

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

vec3 CalculateGerstner(vec3 position) {

	vec2 terrainTex = (vec2(position.xz) - vec2(terrainTranslation.xz))
		/ terrainSideLength;

	float shoreDistance = texture(terrainHeight, terrainTex).g;
	vec2 shoreGradient = normalize(2.0 * texture(terrainHeight, terrainTex).ba - 1.0);

	// Avoid scaling by water depth. Resolution artifacts become obvious.
	float scale = clamp(1.0 - shoreDistance, 0.0, 1.0);
	scale *= clamp(shoreDistance * 3.0, 0.0, 1.0);

	// Should be uniforms
	float waveLength = shoreWaveLength / 100.0;
	float speed = shoreWaveSpeed / 100.0;

	float w = 1.0 / waveLength;
	float phi = speed * w;

	float rad = w * shoreDistance + phi * time;

	float waveIndex = (rad - 1.5 * PI) / (2.0 * PI);

	float modulation = 0.0;

	if (mod(floor(waveIndex), 3.0) == 0.0) {
		modulation = 1.0;
	}
	if (mod(floor(waveIndex), 8.0) == 0.0) {
		modulation = 1.0;
	}

	float distanceScale = saturate(1.0 - (distance(cameraLocation, position)
		 - shoreWaveDistanceOffset) / shoreWaveDistanceScale);

	float amplitude = shoreWaveAmplitude * scale * modulation * distanceScale;
	float steepness = shoreWaveSteepness * scale * modulation * distanceScale;

	vec3 offset;

	float gamma = shoreWavePower;
	offset.y = amplitude * pow(0.5 * sin(rad) + 0.5, gamma);
	offset.xz = -shoreGradient * steepness * amplitude * pow(0.5 * cos(rad) + 0.5, gamma);

	// Move these calculations to the fragment shader
	foamShoreWave = pow(max(-cos(rad), 0.0), 1.0) * 1.0 * max(scale - 0.3, 0.0) * modulation;
	breakingShoreWave = saturate(pow(max(sin(rad + 0.4), 0.0), 2.0) * 2.0 * max(scale - 0.3, 0.0)) * modulation;

	foamShoreWave = saturate(foamShoreWave);
	breakingShoreWave = saturate(breakingShoreWave);

	return offset;

}

void main() {
	
	waterDepth = shoreStartScaling;
	foamShoreWave = 0.0;
	
	fPosition = stitch(vPosition) * nodeSideLength + 
		vec3(nodeLocation.x, 0.0, nodeLocation.y) + translation;
	
	bool hasTerrain = terrainSideLength > 0.0;

	vec2 terrainTex = (vec2(fPosition.xz) - vec2(terrainTranslation.xz))
		/ terrainSideLength;
		
	float shoreDistance = 0.0;
	vec2 shoreGradient = vec2(0.0);

	if (hasTerrain && terrainTex.x >= 0.0 && terrainTex.y >= 0.0
		&& terrainTex.x <= 1.0 && terrainTex.y <= 1.0) {
		waterDepth = fPosition.y - texture(terrainHeight, terrainTex).r 
			* terrainHeightScale + terrainTranslation.y;
		shoreDistance = texture(terrainHeight, terrainTex).g;
		shoreGradient = normalize(2.0 * texture(terrainHeight, terrainTex).ba - 1.0);
	}
	
	float depthScaling = clamp((waterDepth - shoreOffsetScaling) / 
		(shoreStartScaling - shoreOffsetScaling), minShoreScaling, 1.0);
	shoreScaling = hasTerrain ? depthScaling : 1.0;
	
	vec2 vTexCoord = vec2(fPosition.x, fPosition.z) / tiling;

	fOriginalCoord = fPosition;
	
	fPosition.y += texture(displacementMap, vTexCoord).r * displacementScale * shoreScaling;
	fPosition.x += texture(displacementMap, vTexCoord).g * choppyScale * shoreScaling;
	fPosition.z += texture(displacementMap, vTexCoord).b * choppyScale * shoreScaling;

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