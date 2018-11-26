#include "../structures"
#include "convert"

in vec3 fTexCoordProj;
out vec4 fragColor;

uniform sampler2D diffuseTexture;
uniform sampler2D normalTexture;
uniform sampler2D materialTexture;
uniform sampler2D depthTexture;

uniform Light light;
uniform vec3 viewSpaceLightLocation;

void main() {

	vec2 texCoord = ((fTexCoordProj.xy / fTexCoordProj.z) + 1.0f) / 2.0f;
	
	float depth = texture(depthTexture, texCoord).r;
	
	vec3 fragPos = ConvertDepthToViewSpace(depth, texCoord);

	if (fragPos.z  - viewSpaceLightLocation.z > light.radius)
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
	
	float occlusionFactor = 1.0f;
	
	vec3 lightDir = fragToLight / fragToLightDistance;
	
	diffuse = max((dot(normal, lightDir) * light.color) * shadowFactor,
		ambient * occlusionFactor) * surfaceColor;

	fragColor = max(vec4((diffuse + ambient) * (light.radius - fragToLightDistance) / light.radius, 1.0f), 0.0f);

}