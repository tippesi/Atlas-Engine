#include <structures>
#include <common/convert.hsh>

in vec2 fTexCoord;

layout(binding = 0) uniform sampler2D depthTexture;
layout(binding = 1) uniform sampler2DArrayShadow cascadeMaps;

uniform Light light;
uniform int sampleCount;
uniform vec2 framebufferResolution;

out float foginess;

float ComputeVolumetric(vec3 fragPos, vec2 texCoords);

void main() {

    float depth = textureLod(depthTexture, fTexCoord, 0).r;

    vec3 fragPos = ConvertDepthToViewSpace(depth, fTexCoord);

    foginess = ComputeVolumetric(fragPos, fTexCoord);

}

/*
// Henyey-Greenstein phase function https://www.astro.umd.edu/~jph/HG_note.pdf
float ComputeScattering(float lightDotView) {
    // Range [-1;1]
    float g = scattering;
    float gSquared = g * g;
    float result = 1.0 -  gSquared;
    result /= (4.0 * 3.14 * pow(1.0 + gSquared - (2.0 * g) * lightDotView, 1.5));
    return result;
}
*/

const float ditherPattern[16] = float[](0.0, 0.5, 0.125, 0.625, 0.75, 0.22, 0.875, 0.375,
		0.1875, 0.6875, 0.0625, 0.5625, 0.9375, 0.4375, 0.8125, 0.3125);

float ComputeVolumetric(vec3 fragPos, vec2 texCoords) {

    // We compute this in view space
    vec3 rayVector = fragPos;
    float rayLength = length(rayVector);
    vec3 rayDirection = rayVector / rayLength;
    float stepLength = rayLength / float(sampleCount);
    vec3 stepVector = rayDirection * stepLength;
 
    float foginess = 0.0;
	
	texCoords = (0.5 * texCoords + 0.5) * framebufferResolution;
	
	float ditherValue = ditherPattern[(int(texCoords.x) % 4) * 4 + int(texCoords.y) % 4];
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

        float shadowValue = textureGrad(cascadeMaps, 
            vec4(cascadeSpace.xy, cascadeIndex, cascadeSpace.z), 
            vec2(0, 0),
    	    vec2(0, 0));
        foginess += shadowValue;

        currentPosition += stepVector;

    }

    float shadowDistance = light.shadow.cascades[light.shadow.cascadeCount - 1].distance;
    float scale = min(1.0, rayLength / shadowDistance);

    return foginess / float(sampleCount) * scale;

}