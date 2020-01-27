#define SHADOW_FILTER_7x7
#define SHADOW_CASCADE_BLENDING

#include "../structures"
#include "../shadow"
#include "../fog"

#include <../common/convert>

in vec2 fTexCoord;
out vec4 fragColor;

layout(binding = 0) uniform sampler2D diffuseTexture;
layout(binding = 1) uniform sampler2D normalTexture;
layout(binding = 2) uniform sampler2D materialTexture;
layout(binding = 3) uniform sampler2D geometryNormalTexture;
layout(binding = 4) uniform sampler2D emissionTexture;
layout(binding = 5) uniform sampler2D depthTexture;
layout(binding = 6) uniform sampler2D aoTexture;
layout(binding = 7) uniform sampler2D volumetricTexture;

uniform Light light;

uniform mat4 ivMatrix;
uniform vec3 cameraLocation;
uniform vec3 cameraDirection;

uniform float aoStrength;

void main() {
	
	float depth = texture(depthTexture, fTexCoord).r;
	
	if (depth == 1.0)
		discard;
		
	vec3 emissive = texture(emissionTexture, fTexCoord).rgb;
	
	vec3 fragPos = ConvertDepthToViewSpace(depth, fTexCoord);	
	vec3 normal = normalize(2.0 * texture(normalTexture, fTexCoord).rgb - 1.0);

	vec3 reconstructedNormal = normalize(2.0 * texture(geometryNormalTexture, fTexCoord).rgb - 1.0);
	
	vec3 surfaceColor = texture(diffuseTexture, fTexCoord).rgb;
	vec2 material = texture(materialTexture, fTexCoord).rg;
	
	// Material properties
	float specularIntensity = max(material.r, 0.0);
	float specularHardness = max(material.g, 1.0);
	
	float shadowFactor = 1.0;
	vec3 volumetric = vec3(0.0);
	
	vec3 viewDir = normalize(-fragPos);
	vec3 lightDir = -light.direction;
	
	float nDotL = max(dot(normal, lightDir), 0.0);
	
#ifdef SHADOWS	
	shadowFactor = CalculateCascadedShadow(light, fragPos,
		reconstructedNormal, nDotL); 
	
	volumetric = texture(volumetricTexture, fTexCoord).r * light.color * light.scatteringFactor;
#endif

	vec3 specular = vec3(1.0);
	vec3 diffuse = surfaceColor;
	vec3 ambient = light.ambient * surfaceColor;
	
	float occlusionFactor = 1.0;
	
#ifdef SSAO
	occlusionFactor = pow(texture(aoTexture, fTexCoord).r, aoStrength);
	ambient *= occlusionFactor;
#endif
		
	vec3 halfway = normalize(lightDir + viewDir);
	specular *= pow(max(dot(normal, halfway), 0.0), specularHardness)
		* specularIntensity;
	
	fragColor = vec4((diffuse + specular) * 
		nDotL * light.color * shadowFactor
		+ ambient + volumetric, 1.0);
	
#ifndef AE_API_GLES
	if (length(emissive) > 0.01) {	
		fragColor = vec4(emissive, 1.0);
	}
#endif
	
	fragColor = vec4(applyFog(fragColor.rgb, length(fragPos), 
		cameraLocation, mat3(ivMatrix) * -viewDir, 
		mat3(ivMatrix) * -light.direction, light.color), 1.0);
}