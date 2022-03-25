#define SHADOW_FILTER_1x1

#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../structures>
#include <../shadow.hsh>

#include <sharedUniforms.hsh>
#include <shoreInteraction.hsh>

// Lighting based on Island Demo (NVIDIA SDK 11)

layout (location = 0) out vec3 color;
layout (location = 1) out vec2 velocity;

layout(binding = 1) uniform sampler2D normalMap;
layout (binding = 2) uniform sampler2D foamTexture;
layout (binding = 3) uniform samplerCube skyEnvProbe;
layout (binding = 4) uniform sampler2D refractionTexture;
layout (binding = 5) uniform sampler2D depthTexture;
layout (binding = 7) uniform sampler2D volumetricTexture;
layout (binding = 10) uniform sampler2D rippleTexture;

in vec4 fClipSpace;
in vec3 fPosition;
in vec3 fModelCoord;
in vec3 fOriginalCoord;
in vec2 fTexCoord;
in float waterDepth;
in float shoreScaling;
in vec3 ndcCurrent;
in vec3 ndcLast;
in vec3 normalShoreWave;

uniform vec3 translation;
uniform mat4 ivMatrix;
uniform mat4 vMatrix;

uniform vec2 jitterLast;
uniform vec2 jitterCurrent;

uniform bool hasRippleTexture;

uniform Light light;

uniform vec3 waterBodyColor = pow(vec3(0.1, 1.0, 0.7), vec3(2.2));
uniform vec3 deepWaterBodyColor = pow(vec3(0.1,0.15, 0.5), vec3(2.2));
uniform vec3 scatterColor = pow(vec3(0.3,0.7,0.6), vec3(2.2));
uniform vec2 waterColorIntensity = pow(vec2(0.4, 0.6), vec2(2.2));

// Control water scattering at crests
const float scatterIntensity = 1.5;
const float scatterCrestScale = 0.2;
const float scatterCrestOffset = 0.0;

// Specular properties
const float specularPower = 500.0;
const float specularIntensity = 350.0;

// Shore softness (lower is softer)
const float shoreSoftness = 7.5;

void main() {
	
	// Retrieve precalculated normals and wave folding information
	vec3 fNormal = normalize(2.0 * texture(normalMap, fTexCoord).rgb - 1.0);
	float fold = texture(normalMap, fTexCoord).a;
	
	vec2 ndcCoord = 0.5 * (fClipSpace.xy / fClipSpace.w) + 0.5;
	float clipDepth = textureLod(depthTexture, ndcCoord, 0.0).r;
	
	vec3 depthPos = ConvertDepthToViewSpace(clipDepth, ndcCoord);
	
	float shadowFactor = max(CalculateCascadedShadow(light, fPosition, fNormal, 1.0), 0.01);
	
	fNormal = mix(normalShoreWave, fNormal, shoreScaling);

	// Create TBN matrix for normal mapping
	vec3 norm = fNormal;
	vec3 tang = vec3(1.0, 0.0, 0.0);
	tang.y = -((norm.x*tang.x) / norm.y) - ((norm.z*tang.z) / norm.y);
	tang = normalize(tang);
	vec3 bitang = normalize(cross(tang, norm));
	mat3 tbn = mat3(tang, bitang, norm);
	
	// Normal mapping normal (offsets actual normal)
	vec3 rippleNormal = vec3(0.0, 1.0, 0.0);

	if (hasRippleTexture) {
		rippleNormal = normalize(2.0 * texture(rippleTexture, 200.0 * fTexCoord - vec2(time * 0.2)).rgb - 1.0);
		rippleNormal += normalize(2.0 * texture(rippleTexture, 200.0 * fTexCoord * 0.5 + vec2(time * 0.05)).rgb - 1.0);
		// Won't work with rippleNormal = vec3(0.0, 1.0, 0.0). Might be worth an investigation
		norm = normalize(tbn * rippleNormal);
	}
	
	// Scale ripples based on actual (not view) depth of water
	float rippleScaling = clamp(1.0 - shoreScaling, 0.2, 0.7);
	norm = normalize(mix(fNormal, norm, rippleScaling));

	vec3 eyeDir = normalize(fModelCoord - cameraLocation);

	float nDotL = dot(norm, -light.direction);
	float nDotE = saturate(dot(norm, -eyeDir));

	// Calculate fresnel factor
	float fresnel = 0.02 + (1.0 - 0.02) * pow(1.0 - nDotE, 5.0);
	
	// Calculate reflection vector	
	vec3 reflectionVec = reflect(eyeDir, norm);
	reflectionVec.y = max(0.0, reflectionVec.y);
	
	// Calculate sun spot
	float specularFactor = shadowFactor * fresnel * pow(max(dot(reflectionVec,
	 	-light.direction), 0.0), specularPower);

	// Scattering equations
	float waveHeight = fModelCoord.y - translation.y;
	float scatterFactor = scatterIntensity * max(0.0, waveHeight
		 * scatterCrestScale + scatterCrestOffset);

	scatterFactor *= shadowFactor * pow(max(0.0, dot(normalize(vec3(-light.direction.x,
		0.0, -light.direction.z)), eyeDir)), 2.0);
	
	scatterFactor *= pow(max(0.0, 1.0 - nDotL), 8.0);

	//scatterFactor += shadowFactor * 2.5 * waterColorIntensity.y
	//	 * max(0.0, waveHeight) * max(0.0, nDotE) * 
	//	 max(0.0, 1.0 + eyeDir.y);

	// Calculate water depth based on the viewer (ray from camera to ground)
	float waterViewDepth = max(0.0, fPosition.z - depthPos.z);
	
	vec2 disturbance = (mat3(vMatrix) * vec3(norm.x, 0.0, norm.z)).xz;

	vec2 refractionDisturbance = vec2(-disturbance.x, disturbance.y) * 0.05;
	refractionDisturbance *= min(2.0, waterViewDepth);

	// Retrieve colors from textures
	vec3 refractionColor = texture(refractionTexture, ndcCoord + refractionDisturbance).rgb;
	vec3 reflectionColor = texture(skyEnvProbe, reflectionVec).rgb;

	// Calculate water color
	float diffuseFactor = waterColorIntensity.x + waterColorIntensity.y * 
		max(0.0, nDotL) * shadowFactor;
	vec3 waterColor = diffuseFactor * light.color * mix(deepWaterBodyColor, waterBodyColor, min(1.0 , exp(-waterViewDepth / 10.0)));
	
	// Water edges at shore sould be soft
	fresnel *= min(1.0, waterViewDepth * shoreSoftness);

	// Update refraction color based on water depth (exponential falloff)
	refractionColor = mix(waterColor, refractionColor, min(1.0 , exp(-waterViewDepth / 2.0)));
	
	vec2 shoreInteraction = CalculateShoreInteraction(fModelCoord);

	// Calculate foam based on folding of wave and fade it out near shores
	float foam = 0.0;
	foam += shoreInteraction.x;
	foam = min(foam, 1.0);

	// Fade the reflection out caused by foam
	reflectionColor *= 1.0 - 1.0 * foam;

	// Mix relection and refraction and add sun spot
	//color = vec3(fresnel);
	color = mix(refractionColor, reflectionColor, fresnel);
	color += specularIntensity * fresnel * specularFactor * light.color;
	color += scatterColor * scatterFactor;
	
	color = mix(color, vec3(mix(scatterColor * 0.1, vec3(1.0), 
		texture(foamTexture, fOriginalCoord.xz / 8.0).r)), foam);

	vec3 breakingColor = mix(vec3(1.0),
		vec3(1.0) * max(0.0, nDotL)* shadowFactor, 0.7);
	color = mix(color, breakingColor, shoreInteraction.y);

	vec2 terrainTex = (vec2(fModelCoord.xz) - vec2(terrainTranslation.xz))
		/ terrainSideLength;

	//color = vec3(texture(terrainHeight, terrainTex).ba, 0.0);
	//color = vec3(shoreScaling);

	// Calculate velocity
	vec2 ndcL = ndcLast.xy / ndcLast.z;
	vec2 ndcC = ndcCurrent.xy / ndcCurrent.z;

	ndcL -= jitterLast;
	ndcC -= jitterCurrent;

	velocity = (ndcL - ndcC) * 0.5;

}