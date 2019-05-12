layout (location = 0) out vec3 diffuse;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec2 additional;

layout (binding = 2) uniform sampler2D foamTexture;
layout (binding = 3) uniform samplerCube skyEnvProbe;

in vec3 fPosition;
in vec3 fNormal;
in vec2 fTexCoord;
in float fold;

uniform vec3 cameraLocation;
uniform mat4 vMatrix;

const vec3 sunDir = vec3(0.936016, 0.0780013, -0.343206);
const vec3 waterBodyColor = vec3(0.07f, 0.15f, 0.2f);

void main() {	
	
	diffuse = waterBodyColor;
		
	normal = normalize(fNormal);
		
	vec3 reflectionVec = normalize(inverse(mat3(vMatrix)) * reflect(normalize(fPosition), normal));
	
	float sunSpot = pow(clamp(dot(reflectionVec, sunDir), 0.0, 1.0), 600.0);
	
	float fresnel = 0.015 + (1.0 - 0.015) * pow(max(0.0, 1.0 - dot(normalize(-fPosition), normal)), 5.0);
	
	diffuse = mix(waterBodyColor, texture(skyEnvProbe, reflectionVec).xyz, min(1.0, fresnel));
	
	diffuse += sunSpot * vec3(1.0, 1.0, 0.6);
	
	float foamIntensity = texture(foamTexture, fTexCoord * 20.0f).r;
	
	diffuse  = mix(diffuse, vec3(foamIntensity), min(1.0, foamIntensity * pow(fold, 2.0)));
	
 	normal = 0.5f * vec3(vMatrix * vec4(0.0, 1.0, 0.0, 0.0)) + 0.5f;
	
	additional = vec2(0.0f, 300.0f);
	
}