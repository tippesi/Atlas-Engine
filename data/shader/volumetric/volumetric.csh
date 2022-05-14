#ifdef AE_TEXTURE_SHADOW_LOD
#extension GL_EXT_texture_shadow_lod : require
#endif

layout (local_size_x = 8, local_size_y = 8) in;

#include <../structures>
#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../common/random.hsh>
#include <fog.hsh>

layout(binding = 0) uniform sampler2D depthTexture;
layout(binding = 1) uniform sampler2DArrayShadow cascadeMaps;
layout(binding = 2) writeonly uniform image2D volumetricImage;

uniform Light light;
uniform int sampleCount;
uniform vec2 framebufferResolution;
uniform float intensity;
uniform mat4 ivMatrix;
uniform float seed;

vec3 ComputeVolumetric(vec3 fragPos, vec2 texCoords);

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(volumetricImage).x ||
        pixel.y > imageSize(volumetricImage).y)
        return;

	vec2 texCoord = (vec2(pixel) + 0.5) / vec2(imageSize(volumetricImage));

    float depth = textureLod(depthTexture, texCoord, 0.0).r;
    vec3 pixelPos = ConvertDepthToViewSpace(depth, texCoord);

    vec3 radiance = ComputeVolumetric(pixelPos, texCoord);
    imageStore(volumetricImage, pixel, vec4(radiance, 0.0));

}

const float ditherPattern[16] = float[](0.0, 0.5, 0.125, 0.625, 0.75, 0.22, 0.875, 0.375,
		0.1875, 0.6875, 0.0625, 0.5625, 0.9375, 0.4375, 0.8125, 0.3125);

vec3 ComputeVolumetric(vec3 fragPos, vec2 texCoords) {

    vec3 viewPosition = vec3(ivMatrix * vec4(fragPos, 1.0));

    // We compute this in view space
    vec3 rayVector = fragPos;
    float rayLength = length(rayVector);
    vec3 rayDirection = rayVector / rayLength;
    float stepLength = rayLength / float(sampleCount);
    vec3 stepVector = rayDirection * stepLength;
 
    vec3 foginess = vec3(0.0);
	
	texCoords = (0.5 * texCoords + 0.5) * framebufferResolution;
	
    float rndSeed = seed;
    float rnd0 = random(texCoords, rndSeed) * 0.0;
    float rnd1 = random(texCoords, rndSeed) * 0.0;
	float ditherValue = ditherPattern[(int(texCoords.x + rnd0) % 4) * 4 + int(texCoords.y + rnd1) % 4];
   
	vec3 currentPosition = stepVector * ditherValue;

	int cascadeIndex = 0;
	int lastCascadeIndex = 0;
	mat4 cascadeMatrix = light.shadow.cascades[0].cascadeSpace;

    for (int i = 0; i < sampleCount; i++) {
        
		float distance = -currentPosition.z;
		
		int cascadeIndex = 0;
        
        cascadeIndex = distance >= light.shadow.cascades[0].distance ? 1 : cascadeIndex;
        cascadeIndex = distance >= light.shadow.cascades[1].distance ? 2 : cascadeIndex;
        cascadeIndex = distance >= light.shadow.cascades[2].distance ? 3 : cascadeIndex;
        cascadeIndex = distance >= light.shadow.cascades[3].distance ? 4 : cascadeIndex;
        cascadeIndex = distance >= light.shadow.cascades[4].distance ? 5 : cascadeIndex;
        
        cascadeIndex = min(light.shadow.cascadeCount - 1, cascadeIndex);

        if (lastCascadeIndex != cascadeIndex) {
            cascadeMatrix = light.shadow.cascades[0].cascadeSpace;
            cascadeMatrix = cascadeIndex > 0 ? light.shadow.cascades[1].cascadeSpace : cascadeMatrix;
            cascadeMatrix = cascadeIndex > 1 ? light.shadow.cascades[2].cascadeSpace : cascadeMatrix;
            cascadeMatrix = cascadeIndex > 2 ? light.shadow.cascades[3].cascadeSpace : cascadeMatrix;
            cascadeMatrix = cascadeIndex > 3 ? light.shadow.cascades[4].cascadeSpace : cascadeMatrix;
            cascadeMatrix = cascadeIndex > 4 ? light.shadow.cascades[5].cascadeSpace : cascadeMatrix;         
        }

		lastCascadeIndex = cascadeIndex;
		
        vec4 cascadeSpace = cascadeMatrix * vec4(currentPosition, 1.0);
        cascadeSpace.xyz /= cascadeSpace.w;

        cascadeSpace.xyz = cascadeSpace.xyz * 0.5 + 0.5;

#ifdef AE_TEXTURE_SHADOW_LOD
        // This fixes issues that can occur at cascade borders
        float shadowValue = textureLod(cascadeMaps, 
            vec4(cascadeSpace.xy, cascadeIndex, cascadeSpace.z), 0);
#else
        float shadowValue = texture(cascadeMaps, 
            vec4(cascadeSpace.xy, cascadeIndex, cascadeSpace.z));
#endif

        vec3 worldPosition = vec3(ivMatrix * vec4(currentPosition, 1.0));
        
        float fogAmount = fogEnabled ? (1.0 - saturate(ComputeVolumetricFog(viewPosition, worldPosition))) : 1.0;
        float NdotL = dot(rayDirection, light.direction);

        foginess += shadowValue * fogAmount * ComputeScattering(NdotL) * light.color;

        currentPosition += stepVector;

    }

    return foginess / float(sampleCount) * intensity;

}