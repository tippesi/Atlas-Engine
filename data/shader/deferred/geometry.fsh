#include <../common/random>

layout (location = 0) out vec4 diffuse;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec2 additional;
layout (location = 3) out vec3 geometryNormal;
layout (location = 4) out vec3 emission;
layout (location = 5) out vec2 velocity;

#ifdef DIFFUSE_MAP
layout(binding = 0) uniform sampler2D diffuseMap;
#endif
#ifdef NORMAL_MAP
layout(binding = 1) uniform sampler2D normalMap;
#endif
#ifdef SPECULAR_MAP
layout(binding = 2) uniform sampler2D specularMap;
#endif
#ifdef HEIGHT_MAP
layout(binding = 3) uniform sampler2D heightMap;
#endif

in vec2 fTexCoord;
in vec3 fNormal;
in vec3 fPosition;
in vec3 ndcCurrent;
in vec3 ndcLast;

#if defined(NORMAL_MAP) || defined(HEIGHT_MAP)
in mat3 toTangentSpace;
#endif


#ifdef REFLECTION
in vec3 fPosition;
#endif

#ifdef REFLECTION
uniform samplerCube environmentCube;
uniform mat4 ivMatrix;
#endif

uniform vec3 diffuseColor;
uniform vec3 emissiveColor;

uniform float specularIntensity;
uniform float specularHardness;

uniform float normalScale;
uniform float displacementScale;

uniform mat4 vMatrix;

uniform vec2 jitterLast;
uniform vec2 jitterCurrent;

vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir) { 
#ifdef HEIGHT_MAP
    // number of depth layers
    const float minLayers = 32.0;
	const float maxLayers = 32.0;
	float numLayers = mix(minLayers, maxLayers,  
		abs(dot(vec3(0.0, 1.0, 0.0), viewDir)));  
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
	vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

	return finalTexCoords;
#else
	return vec2(1.0);
#endif

}

void main() {
	
	vec4 textureColor = vec4(diffuseColor, 1.0);
	
	vec2 texCoords = fTexCoord;
	
	// Check if usage is valid (otherwise texCoords won't be used)
#if defined(HEIGHT_MAP) && (defined(DIFFUSE_MAP) || defined(NORMAL_MAP) || defined(SPECULAR_MAP)) 
	vec3 viewDir = normalize(transpose(toTangentSpace) * -fPosition);
	texCoords = ParallaxMapping(texCoords, viewDir);
#endif
	
#ifdef DIFFUSE_MAP
	textureColor *= texture(diffuseMap, texCoords);	
	if (textureColor.a < 0.2)
		discard;
#endif

	normal = normalize(fNormal);

	geometryNormal = 0.5 * normal + 0.5;

#ifdef NORMAL_MAP
	vec3 normalColor = texture(normalMap, texCoords).rgb;
	normal = mix(normal, normalize(toTangentSpace * (2.0 * normalColor - 1.0)), normalScale);
#endif

#ifdef REFLECTION
    vec3 R = mat3(ivMatrix) * reflect(normalize(fPosition), normal);
    diffuse = vec4(mix(textureColor.rgb, textureLod(environmentCube, R, specularHardness / 50.0).rgb, reflectivity), 1.0);
#else
	diffuse = vec4(textureColor.rgb, 1.0);
#endif

	float specularFactor = 1.0;

#ifdef SPECULAR_MAP
	specularFactor = texture(specularMap, texCoords).r;
#endif

	additional = vec2(specularIntensity * specularFactor, specularHardness);
	
	normal = 0.5 * normal + 0.5;
	
#ifdef EMISSIVE
	emission = clamp(emissiveColor, vec3(0.0), vec3(1.0));
#else
	emission = vec3(0.0);
#endif
	// Calculate velocity
	vec2 ndcL = ndcLast.xy / ndcLast.z;
	vec2 ndcC = ndcCurrent.xy / ndcCurrent.z;

	ndcL -= jitterLast;
	ndcC -= jitterCurrent;

	velocity = (ndcL - ndcC) * 0.5;
	
}