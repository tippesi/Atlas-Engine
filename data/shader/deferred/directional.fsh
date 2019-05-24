#include "../structures"
#include "../shadow"

#include "convert"

in vec2 fTexCoord;
out vec4 fragColor;

layout(binding = 0) uniform sampler2D diffuseTexture;
layout(binding = 1) uniform sampler2D normalTexture;
layout(binding = 2) uniform sampler2D materialTexture;
layout(binding = 3) uniform sampler2D depthTexture;
layout(binding = 4) uniform sampler2D aoTexture;
layout(binding = 5) uniform sampler2D volumetricTexture;

uniform Light light;

uniform mat4 ivMatrix;

uniform float aoStrength;

void main() {
	
	float depth = texture(depthTexture, fTexCoord).r;
	
	if (depth == 1.0f)
		discard;
	
	vec3 fragPos = ConvertDepthToViewSpace(depth, fTexCoord);	
	vec3 normal = normalize(2.0f * texture(normalTexture, fTexCoord).rgb - 1.0f);
	
	vec3 surfaceColor = texture(diffuseTexture, fTexCoord).rgb;
	vec2 material = texture(materialTexture, fTexCoord).rg;
	
	// Material properties
	float specularIntensity = material.r;
	float specularHardness = material.g;
	
	float shadowFactor = 1.0f;
	vec3 volumetric = vec3(0.0f);
	
#ifdef SHADOWS	
	vec3 modelCoords = vec3(ivMatrix * vec4(fragPos, 1.0f));
	
	shadowFactor = CalculateCascadedShadow(light, modelCoords, fragPos); 
	
	volumetric = texture(volumetricTexture, fTexCoord).r * light.color * light.scatteringFactor;
#endif

	vec3 specular = vec3(0.0f);
	vec3 diffuse = vec3(1.0f);
	vec3 ambient = vec3(light.ambient * surfaceColor);
	
	float occlusionFactor = 1.0f;
		
	vec3 viewDir = normalize(-fragPos);
	vec3 lightDir = -light.direction;
	
#ifdef SSAO
	occlusionFactor = pow(texture(aoTexture, fTexCoord).r, aoStrength);
	ambient *= occlusionFactor;
#endif
	
	diffuse = max((dot(normal, lightDir) * light.color) * shadowFactor,
		0.0) * surfaceColor;		
	
	if(specularIntensity > 0.0f && shadowFactor > 0.5f) {
		
		vec3 halfwayDir = normalize(lightDir + viewDir);  
		float dampedFactor = pow(max(dot(normal, halfwayDir), 0.0f), specularHardness);
		specular = light.color * dampedFactor * specularIntensity;
	
	}
	
	if(shadowFactor < 0.5f)
		fragColor = vec4(diffuse + ambient + volumetric, 1.0f);
	else
		fragColor = vec4(diffuse + specular + ambient + volumetric, 1.0f);

}