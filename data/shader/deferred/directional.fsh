#include "../structures"
#include "../shadow"

#include "convert"

in vec2 fTexCoord;
out vec3 fragColor;

uniform sampler2D diffuseTexture;
uniform sampler2D normalTexture;
uniform sampler2D materialTexture;
uniform sampler2D depthTexture;
uniform sampler2D aoTexture;

uniform Light light;

uniform mat4 lightSpaceMatrix;//Is pre-multiplied with the inverseViewMatrix
uniform mat4 ivMatrix;

uniform float aoStrength;

// Henyey-Greenstein phase function https://www.astro.umd.edu/~jph/HG_note.pdf
float ComputeScattering(float lightDotView) {
	// Range [-1;1]
	const float g = 0.0f;
	float gSquared = g * g;
	float result = 1.0f -  gSquared;
	result /= (4.0f * 3.14f * pow(1.0f + gSquared - (2.0f * g) * lightDotView, 1.5f));
	return result;
}

vec3 ComputeVolumetric(vec3 fragPos) {
	
	const int steps = 100;
	
	vec4 cascadesDistance = vec4(light.shadow.cascades[0].distance,
								light.shadow.cascades[1].distance,
								light.shadow.cascades[2].distance,
								light.shadow.cascades[3].distance);
							
	vec4 cascadeCount = vec4(light.shadow.cascadeCount > 0,
							light.shadow.cascadeCount > 1,
							light.shadow.cascadeCount > 2,
							light.shadow.cascadeCount > 3);
	
	// We compute this in view space
	vec3 rayVector = fragPos;
	float rayLength = length(rayVector);
	vec3 rayDirection = rayVector / rayLength;
	float stepLength = rayLength / steps;
	vec3 stepVector = rayDirection * stepLength;
	vec3 currentPosition = vec3(0.0f);
	float foginess = 0.0f;
	
	for (int i = 0; i < steps; i++) {
		vec4 comparison = vec4(-currentPosition.z > cascadesDistance.x,
							-currentPosition.z > cascadesDistance.y,
							-currentPosition.z > cascadesDistance.z,
							-currentPosition.z > cascadesDistance.w);
		float fIndex = dot(cascadeCount, comparison);
		int index = int(fIndex);
		vec4 cascadeSpace = light.shadow.cascades[index].cascadeSpace * vec4(currentPosition, 1.0f);
		cascadeSpace.xyz /= cascadeSpace.w;
		
		cascadeSpace.xyz = cascadeSpace.xyz * 0.5f + 0.5f;
		
		float shadowValue = texture(cascadeMaps, vec4(cascadeSpace.xy, index, cascadeSpace.z));
		foginess += ComputeScattering(dot(rayDirection, light.direction)) * shadowValue;
		
		currentPosition += stepVector;
		
	}
	
	return (foginess / steps) * light.color;
	
}

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
	
	volumetric = ComputeVolumetric(fragPos);
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
	
	diffuse = max((dot(normal, lightDir) * light.color) * shadowFactor, ambient * occlusionFactor)
		* surfaceColor;		
	
	if(specularIntensity > 0.0f && shadowFactor > 0.5f) {
		
		vec3 halfwayDir = normalize(lightDir + viewDir);  
		float dampedFactor = pow(max(dot(normal, halfwayDir), 0.0f), specularHardness);
		specular = light.color * dampedFactor * specularIntensity;
	
	}
	
	if(shadowFactor < 0.5f)
		fragColor = diffuse + ambient + volumetric;
	else
		fragColor = diffuse + specular + ambient + volumetric;

}