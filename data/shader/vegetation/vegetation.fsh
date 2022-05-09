#include <../common/random.hsh>

layout (location = 0) out vec4 baseColorFS;
layout (location = 2) out vec3 geometryNormalFS;
layout (location = 4) out uint materialIdxFS;
layout (location = 5) out vec2 velocityFS;

#ifdef BASE_COLOR_MAP
layout(binding = 0) uniform sampler2D baseColorMap;
#endif
#ifdef OPACITY_MAP
layout(binding = 1) uniform sampler2D opacityMap;
#endif
#ifdef NORMAL_MAP
layout(binding = 2) uniform sampler2D normalMap;
#endif
#ifdef ROUGHNESS_MAP
layout(binding = 3) uniform sampler2D roughnessMap;
#endif
#ifdef METALNESS_MAP
layout(binding = 4) uniform sampler2D metalnessMap;
#endif
#ifdef AO_MAP
layout(binding = 5) uniform sampler2D aoMap;
#endif
#ifdef HEIGHT_MAP
layout(binding = 6) uniform sampler2D heightMap;
#endif

in vec3 positionVS;
in vec3 normalVS;
in vec2 texCoordVS;

in vec3 ndcCurrentVS;
in vec3 ndcLastVS;

#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
in mat3 TBN;
#endif

uniform float normalScale;
uniform float displacementScale;

uniform mat4 vMatrix;

uniform vec2 jitterLast;
uniform vec2 jitterCurrent;

uniform uint materialIdx;

void main() {
	
	vec2 texCoords = texCoordVS;

#ifdef OPACITY_MAP
	float opacity = texture(opacityMap, texCoords).r;
	if (opacity < 0.1)
		discard;
#endif

#ifdef BASE_COLOR_MAP
	vec3 textureColor = texture(baseColorMap, texCoords).rgb;
	baseColorFS = vec4(textureColor.rgb, opacity);
#endif
	geometryNormalFS = normalize(normalVS);
	geometryNormalFS = 0.5 * geometryNormalFS + 0.5;

	float roughnessFactor = 1.0;
	float metalnessFactor = 1.0;
	float aoFactor = 1.0;

#ifdef ROUGHNESS_MAP
	roughnessFactor *= texture(roughnessMap, texCoords).r;
	roughnessMetalnessAoFS.r = roughnessFactor;
#endif
#ifdef METALNESS_MAP
	metalnessFactor *= texture(metalnessMap, texCoords).r;
	roughnessMetalnessAoFS.g = metalnessFactor;
#endif
#ifdef AO_MAP
	aoFactor *= texture(aoMap, texCoords).r;
	roughnessMetalnessAoFS.b = aoFactor;
#endif

	// Calculate velocity
	vec2 ndcL = ndcLastVS.xy / ndcLastVS.z;
	vec2 ndcC = ndcCurrentVS.xy / ndcCurrentVS.z;

	ndcL -= jitterLast;
	ndcC -= jitterCurrent;

	velocityFS = (ndcL - ndcC) * 0.5;

	materialIdxFS = materialIdx;
	
}