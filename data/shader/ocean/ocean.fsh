#include "../deferred/convert"
#include "../structures"
#include "../shadow"

out vec3 diffuse;

layout (binding = 2) uniform sampler2D foamTexture;
layout (binding = 3) uniform samplerCube skyEnvProbe;
layout (binding = 4) uniform sampler2D refractionTexture;
layout (binding = 5) uniform sampler2D depthTexture;

in vec4 fClipSpace;
in vec3 fPosition;
in vec3 fModelCoord;
in vec3 fNormal;
in vec2 fTexCoord;
in float fold;

uniform vec3 cameraLocation;
uniform mat4 ivMatrix;

uniform Light light;

const vec3 sunDir = vec3(0.936016, 0.0780013, -0.343206);
const vec3 waterBodyColor = vec3(0.07f, 0.15f, 0.2f);

void main() {
	
	vec2 ndcCoord = 0.5 * (fClipSpace.xy / fClipSpace.w) + 0.5;
	float clipDepth = texture(depthTexture, ndcCoord).r;
	
	vec3 depthPos = ConvertDepthToViewSpace(clipDepth, ndcCoord);
	float waterViewDepth = clamp(distance(fPosition, depthPos) / 5.0, 0.0, 1.0);
	diffuse = waterBodyColor;
	
	float shadowFactor = max(CalculateCascadedShadow(light, fModelCoord, fPosition), light.ambient);
	
	vec3 normal = normalize(fNormal);
		
	vec3 reflectionVec = normalize(mat3(ivMatrix) * reflect(normalize(fPosition), normal));
	
	float sunSpot = pow(clamp(dot(reflectionVec, -light.direction), 0.0, 1.0), 600.0);
	
	float fresnel = 0.09 + (1.0 - 0.09) * pow(max(0.0, 1.0 - dot(normalize(-fPosition), normal)), 5.0);
	
	diffuse *= shadowFactor;
	
	diffuse = mix(diffuse, texture(skyEnvProbe, reflectionVec).xyz, min(1.0, fresnel));
	
	diffuse += sunSpot * vec3(1.0, 1.0, 0.6);
	
	float foamIntensity = texture(foamTexture, fTexCoord * 20.0f).r;
	
	diffuse  = mix(diffuse, vec3(foamIntensity) * shadowFactor, min(1.0, foamIntensity * pow(fold, 2.0)));
	
	diffuse = mix(texture(refractionTexture, ndcCoord).rgb, diffuse, waterViewDepth);
	
}