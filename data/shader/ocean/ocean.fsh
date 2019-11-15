#include "../deferred/convert"
#include "../structures"
#include "../shadow"

out vec3 diffuse;

layout(binding = 1) uniform sampler2D normalMap;
layout (binding = 2) uniform sampler2D foamTexture;
layout (binding = 3) uniform samplerCube skyEnvProbe;
layout (binding = 4) uniform sampler2D refractionTexture;
layout (binding = 5) uniform sampler2D depthTexture;
layout (binding = 7) uniform sampler2D volumetricTexture;

in vec4 fClipSpace;
in vec3 fPosition;
in vec3 fModelCoord;
in vec2 fTexCoord;

uniform vec3 cameraLocation;
uniform mat4 ivMatrix;
uniform mat4 vMatrix;

uniform Light light;

const vec3 sunDir = vec3(0.936016, 0.0780013, -0.343206);
const vec3 waterBodyColor = vec3(0.0, 0.015, 0.0375);

void main() {
	
	vec3 fNormal = vec3(vMatrix * vec4(texture(normalMap, fTexCoord).rgb, 0.0f));
	float fold = texture(normalMap, fTexCoord).a;
	
	vec2 ndcCoord = 0.5 * (fClipSpace.xy / fClipSpace.w) + 0.5;
	float clipDepth = texture(depthTexture, ndcCoord).r;
	
	vec3 depthPos = ConvertDepthToViewSpace(clipDepth, ndcCoord);
	float waterViewDepth = clamp(distance(fPosition, depthPos) / 5.0, 0.0, 1.0);
	diffuse = waterBodyColor;
	
	float shadowFactor = max(CalculateCascadedShadow(light, fPosition, 1.0), light.ambient);
	vec3 volumetric = texture(volumetricTexture, ndcCoord).r * light.color * light.scatteringFactor;
	
	vec3 normal = normalize(fNormal);
		
	vec3 reflectionVec = normalize(mat3(ivMatrix) * reflect(normalize(fPosition), normal));
	
	float sunSpot = pow(clamp(dot(reflectionVec, sunDir), 0.0, 1.0), 400.0);
	
	float fresnel = 0.02 + (1.0 - 0.02) * pow(1.0 - dot(normalize(-fPosition), normal), 5.0);
	
	reflectionVec.y = max(reflectionVec.y, 0.0);
	
	diffuse *= shadowFactor;
	
	diffuse = mix(diffuse, texture(skyEnvProbe, reflectionVec).xyz, min(1.0, fresnel));
	
	diffuse += sunSpot * vec3(1.0, 1.0, 0.6);
	
	float foamIntensity = texture(foamTexture, fTexCoord * 20.0f).r;
	
	diffuse  = mix(diffuse, vec3(foamIntensity) * shadowFactor, foamIntensity * min(1.0, fold));
	
	diffuse = mix(texture(refractionTexture, ndcCoord).rgb, diffuse, waterViewDepth) + volumetric;
	
}