#include "../structures"
#include "convert"

in vec3 fTexCoordProj;
in vec3 viewSpacePosition;
out vec4 fragColor;

layout(binding = 0) uniform sampler2D diffuseTexture;
layout(binding = 1) uniform sampler2D normalTexture;
layout(binding = 2) uniform sampler2D materialTexture;
layout(binding = 3) uniform sampler2D depthTexture;
layout(binding = 4) uniform samplerCubeShadow shadowCubemap;

uniform Light light;
uniform vec3 viewSpaceLightLocation;
uniform mat4 lvMatrix;
uniform mat4 lpMatrix;

// Improved filtering: https://kosmonautblog.wordpress.com/2017/03/25/shadow-filtering-for-pointlights/

vec3 sampleOffsetDirections[20] = vec3[]
(
   vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
   vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
   vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
   vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
   vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);

void main() {

	vec2 texCoord = ((fTexCoordProj.xy / fTexCoordProj.z) + 1.0f) / 2.0f;
	
	float depth = texture(depthTexture, texCoord).r;
	
	vec3 fragPos = ConvertDepthToViewSpace(depth, texCoord);

	if (fragPos.z - viewSpaceLightLocation.z > light.radius)
		discard;

	vec3 fragToLight = viewSpaceLightLocation.xyz - fragPos.xyz;
	float fragToLightDistance = length(fragToLight);
	
	vec3 normal = normalize(2.0f * texture(normalTexture, texCoord).rgb - 1.0f);
	vec3 surfaceColor = texture(diffuseTexture, texCoord).rgb;
	vec2 material = texture(materialTexture, texCoord).rg;
	
	// Material properties
	float specularIntensity = material.r;
	float specularHardness = material.g;
	
	float shadowFactor = 1.0f;
	
	vec3 specular = vec3(0.0f);
	vec3 diffuse = vec3(1.0f);
	vec3 ambient = vec3(light.ambient * surfaceColor);
	vec3 volumetric = 0.0f * vec3(light.color);

	float occlusionFactor = 1.0f;
	
	vec3 lightDir = fragToLight / fragToLightDistance;
	
	vec4 lsPosition = lvMatrix * vec4(fragPos, 1.0f);
	vec4 absPosition = abs(lsPosition);
	depth = -max(absPosition.x, max(absPosition.y, absPosition.z));
	vec4 clip = lpMatrix * vec4(0.0f, 0.0f, depth, 1.0f);	
	depth = (clip.z - 0.005f) / clip.w * 0.5f + 0.5f;
	
	int samples  = 20;
	float diskRadius = 0.0075;
	for(int i = 0; i < samples; i++) {
		shadowFactor += texture(shadowCubemap, vec4(lsPosition.xyz + sampleOffsetDirections[i] * diskRadius, depth)); 
	}
	shadowFactor /= float(samples);  
	
	diffuse = max((dot(normal, lightDir) * light.color) * shadowFactor,
		ambient * occlusionFactor) * surfaceColor;

	fragColor = vec4(max((diffuse + ambient) * (light.radius - fragToLightDistance) / light.radius, 0.0f) + volumetric, 1.0f);

}




float ComputeScattering(vec3 fragPos) {

	// We also need to consider that we shouldn't use the radius of the sphere when we're inside
	// the volume but should use the distance of the viewSpacePosition to the camera.
	const int sampleCount = 100;
	const float offset = 0.5f;	// Because of geometry getting cut off by near plane (offset = 2 * nearPlane)
	
	float positionLength = length(viewSpacePosition);
	float fragLength = length(fragPos);
	
	vec3 rayDirection = viewSpacePosition / positionLength;
	
	vec3 rayEnd = vec3(viewSpacePosition.xyz) - rayDirection * offset / 2.0f;
	
	vec3 rayStart = positionLength < light.radius - offset / 2.0f ? vec3(0.0f) : 
		vec3(viewSpacePosition.xyz) - rayDirection * (light.radius - offset / 2.0f);
		
	float rayLength = distance(rayStart, rayEnd);

	float stepLength = rayLength / float(sampleCount);
	vec3 stepVector = rayDirection  * stepLength;

	float foginess = 0.0f;
	float scattering = 0.15f;

	vec3 currentPosition = vec3(rayStart);

	for (int i = 0; i < sampleCount; i++) {

		// Normally we should take a shadow map into consideration
		float fragToLightDistance = length(viewSpaceLightLocation - currentPosition);
		float shadowValue = fragToLightDistance < light.radius ? 1.0f : 0.0f;
		shadowValue = fragLength < length(currentPosition) ? 0.0f : shadowValue;

		foginess += max(scattering * shadowValue * (light.radius - fragToLightDistance - 0.25f) / (light.radius- 0.25f), 0.0f);

		currentPosition += stepVector;

	}

	return foginess / float(sampleCount);

}