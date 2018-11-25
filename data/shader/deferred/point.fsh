#include "../structures"
#include "convert"

in vec2 fTexCoord;
in vec4 gl_FragCoord;
out vec4 fragColor;

uniform sampler2D diffuseTexture;
uniform sampler2D normalTexture;
uniform sampler2D materialTexture;
uniform sampler2D depthTexture;

uniform Light light;
uniform vec3 viewSpaceLightLocation;

void main() {
	
	float depth = texture(depthTexture, fTexCoord).r;
	
	vec3 fragPos = ConvertDepthToViewSpace(depth, fTexCoord);
	
	vec3 fragToLight = viewSpaceLightLocation.xyz - fragPos.xyz;
	float fragToLightDistance = length(fragToLight);
	
	if (fragToLightDistance > light.radius)
		discard;
	
	vec3 normal = normalize(2.0f * texture(normalTexture, fTexCoord).rgb - 1.0f);
	vec3 surfaceColor = texture(diffuseTexture, fTexCoord).rgb;
	vec2 material = texture(materialTexture, fTexCoord).rg;
	
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
	
	fragColor = vec4((diffuse + ambient) * (light.radius - fragToLightDistance) / light.radius, 1.0f);
	
}