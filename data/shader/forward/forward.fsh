#include "../structures"
#include "../shadow"

out vec3 fragColor;

/*
Uniforms
*/
// The light to be rendered
uniform Light light;

// Material parameter
uniform sampler2D diffuseMap;
uniform sampler2D normalMap;

uniform bool useDiffuseMap;
uniform bool useNormalMap;

uniform vec3 diffuseColor;
uniform float specularIntensity;
uniform float specularHardness;
uniform float reflectivity;

// Special uniforms
#ifdef REFLECTION
uniform samplerCube environmentCube;
uniform mat4 ivMatrix;
#endif

#ifdef SHADOWS
uniform mat4 lightSpaceMatrix;//Is pre-multiplied with the inverseViewMatrix
#endif

/*
Input from the vertex shader
*/
in vec2 fTexCoord;
in vec3 fPosition;
#ifndef FLAT
in vec3 fNormal;
#endif

#ifdef NORMALMAPPING
in mat3 toTangentSpace;
#endif

#ifdef SHADOWS
in vec3 modelPosition;
#endif

void main() {
	
	vec4 textureColor = vec4(diffuseColor, 1.0f);
	
	if(useDiffuseMap)
		textureColor *= texture(diffuseMap, fTexCoord);
	
	
	if(textureColor.a < 0.2f)
		discard;
	
#ifdef FLAT
	// We have to do this complicated because some devices dont support dFdx on 3 component vectors
 	vec3 fdx = vec3(dFdx(fPosition.x),dFdx(fPosition.y),dFdx(fPosition.z));
 	vec3 fdy = vec3(dFdy(fPosition.x),dFdy(fPosition.y),dFdy(fPosition.z));
 	vec3 normal = cross(fdx, fdy);
#else
	vec3 normal = fNormal;
#endif
	
#if defined(NORMALMAPPING) && !defined(FLAT)
	if(useNormalMap)
		normal = normalize(toTangentSpace * (2.0f * texture(normalMap, fTexCoord).rgb - 1.0f));
	else
		normal = normalize(normal);
#else
	normal = normalize(normal);
#endif

#ifdef REFLECTION
    vec3 R = mat3(ivMatrix) * reflect(normalize(fPosition), normal);
    vec3 surfaceColor = vec3(mix(textureColor.rgb, textureLod(environmentCube, R, 1.0f).rgb, reflectivity));
#else
	vec3 surfaceColor = textureColor.rgb;
#endif
	
	float shadowFactor = 1.0f;
	
#ifdef SHADOWS
	vec4 shadowCoords = lightSpaceMatrix * vec4(fPosition, 1.0f);
	shadowCoords.z -= light.shadow.bias;
	shadowCoords.xyz /= shadowCoords.w;
	shadowCoords.w = clamp((length(fPosition) - light.shadow.distance) * 0.1f, 0.0f, 1.0f);
	
	shadowFactor = CalculateShadow(light, modelPosition, shadowCoords);
#endif

	vec3 specular = vec3(0.0f);
	vec3 ambient = vec3(light.ambient * surfaceColor);
		
	vec3 viewDir = normalize(-fPosition);
	vec3 lightDir = normalize(light.position - fPosition);
	
	vec3 diffuse = max((dot(normal, lightDir) * light.color) * shadowFactor, ambient)
		* surfaceColor;	
	
	if(specularIntensity > 0.0f) {
		
		vec3 halfwayDir = normalize(lightDir + viewDir);  
		float dampedFactor = pow(max(dot(normal, halfwayDir), 0.0f), specularHardness);
		specular = light.color * dampedFactor * specularIntensity;
	
	}
	
	if(shadowFactor < 0.5f)
		fragColor = diffuse + ambient;
	else
		fragColor = diffuse + specular + ambient;
	
}