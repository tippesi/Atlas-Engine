#include <../common/random.hsh>

#ifdef GENERATE_IMPOSTOR
layout (location = 0) out vec4 baseColorFS;
#else
layout (location = 0) out vec3 baseColorFS;
#endif
layout (location = 1) out vec3 normalFS;
layout (location = 2) out vec3 geometryNormalFS;
layout (location = 3) out vec3 roughnessMetalnessAoFS;
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

#ifdef GENERATE_IMPOSTOR
uniform vec3 baseColor;
uniform float roughness;
uniform float metalness;
uniform float ao;
#endif

uniform float normalScale;
uniform float displacementScale;

uniform mat4 vMatrix;

uniform vec2 jitterLast;
uniform vec2 jitterCurrent;

uniform uint materialIdx;
uniform bool twoSided;

vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir) { 
#ifdef HEIGHT_MAP
    // number of depth layers (changes are a bit distracting right now)
    const float minLayers = 32.0;
	const float maxLayers = 32.0;
	float numLayers = 16.0;  
    // calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    vec2 P = viewDir.xy / viewDir.z * displacementScale; 
    vec2 deltaTexCoords = P / numLayers;
	vec2  currentTexCoords = texCoords;
	
	vec2 ddx = dFdx(texCoords);
	vec2 ddy = dFdy(texCoords);
	
	float currentDepthMapValue = 1.0 - textureGrad(heightMap, currentTexCoords, ddx, ddy).r;
	
	while(currentLayerDepth < currentDepthMapValue) {
		// shift texture coordinates along direction of P
		currentTexCoords -= deltaTexCoords;
		// get depthmap value at current texture coordinates
		currentDepthMapValue = 1.0 - textureGrad(heightMap, currentTexCoords, ddx, ddy).r;  
		// get depth of next layer
		currentLayerDepth += layerDepth;  
	}
	
	vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

	// get depth after and before collision for linear interpolation
	float afterDepth  = currentDepthMapValue - currentLayerDepth;
	float beforeDepth = 1.0 - textureGrad(heightMap, prevTexCoords, ddx, ddy).r - currentLayerDepth + layerDepth;
	 
	// interpolation of texture coordinates
	float weight = afterDepth / (afterDepth - beforeDepth);
	vec2 finalTexCoords = mix(currentTexCoords, prevTexCoords, weight);

	return finalTexCoords;
#else
	return vec2(1.0);
#endif

}

void main() {
	
	vec2 texCoords = texCoordVS;
	
	// Check if usage is valid (otherwise texCoords won't be used)
#if defined(HEIGHT_MAP) && (defined(BASE_COLOR_MAP) || defined(NORMAL_MAP) || defined(ROUGHNESS_MAP) || defined(METALNESS_MAP) || defined(AO_MAP)) 
	vec3 viewDir = normalize(transpose(TBN) * -positionVS);
	texCoords = ParallaxMapping(texCoords, viewDir);
#endif

#ifdef GENERATE_IMPOSTOR
	baseColorFS = vec4(1.0);
#endif

#ifdef OPACITY_MAP
	float opacity = texture(opacityMap, texCoords).r;
	if (opacity < 0.2)
		discard;
#endif

#ifdef BASE_COLOR_MAP
	vec3 textureColor = texture(baseColorMap, texCoords).rgb;
#ifdef GENERATE_IMPOSTOR
	baseColorFS *= vec4(textureColor.rgb, 1.0);
#else
	baseColorFS = textureColor.rgb;
#endif
#endif

#ifdef GENERATE_IMPOSTOR
	baseColorFS *= vec4(baseColor, 1.0);
#endif

	geometryNormalFS = normalize(normalVS);

#ifdef NORMAL_MAP
	vec3 normalColor = texture(normalMap, texCoords).rgb;
	normalFS = mix(geometryNormalFS, normalize(TBN * (2.0 * normalColor - 1.0)), normalScale);
	// We want the normal always to face the camera for two sided materials
	geometryNormalFS *= twoSided ? dot(normalVS, positionVS) > 0.0 ? -1.0 : 1.0 : 1.0;
	normalFS = 0.5 * normalFS + 0.5;
#else
	// We want the normal always to face the camera for two sided materials
	geometryNormalFS *= twoSided ? dot(normalVS, positionVS) > 0.0 ? -1.0 : 1.0 : 1.0;
#endif
	
	geometryNormalFS = 0.5 * geometryNormalFS + 0.5;

#ifdef GENERATE_IMPOSTOR
	float roughnessFactor = roughness;
	float metalnessFactor = metalness;
	float aoFactor = ao;
#else
	float roughnessFactor = 1.0;
	float metalnessFactor = 1.0;
	float aoFactor = 1.0;
#endif

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

#ifdef GENERATE_IMPOSTOR
	roughnessMetalnessAoFS = vec3(roughnessFactor,
		metalnessFactor, aoFactor);
#endif
	// Calculate velocity
	vec2 ndcL = ndcLastVS.xy / ndcLastVS.z;
	vec2 ndcC = ndcCurrentVS.xy / ndcCurrentVS.z;

	ndcL -= jitterLast;
	ndcC -= jitterCurrent;

	velocityFS = (ndcL - ndcC) * 0.5;

	materialIdxFS = materialIdx;
	
}