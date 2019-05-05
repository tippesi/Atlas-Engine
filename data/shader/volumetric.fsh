#include "structures"
#include "deferred/convert"

in vec2 fTexCoord;

layout(binding = 0) uniform sampler2D depthTexture;
layout(binding = 1) uniform sampler2DArrayShadow cascadeMaps;

uniform Light light;
uniform int sampleCount;
uniform float scattering;
uniform vec2 framebufferResolution;

out float foginess;

float ComputeVolumetric(vec3 fragPos, vec2 texCoords);

void main() {

    float depth = texture(depthTexture, fTexCoord).r;

    vec3 fragPos = ConvertDepthToViewSpace(depth, fTexCoord);

    foginess = ComputeVolumetric(fragPos, fTexCoord);

}

// Henyey-Greenstein phase function https://www.astro.umd.edu/~jph/HG_note.pdf
float ComputeScattering(float lightDotView) {
    // Range [-1;1]
    float g = scattering;
    float gSquared = g * g;
    float result = 1.0f -  gSquared;
    result /= (4.0f * 3.14f * pow(1.0f + gSquared - (2.0f * g) * lightDotView, 1.5f));
    return result;
}

const float ditherPattern[16] = float[](0.0f, 0.5f, 0.125f, 0.625f, 0.75f, 0.22f, 0.875f, 0.375f,
		0.1875f, 0.6875f, 0.0625f, 0.5625, 0.9375f, 0.4375f, 0.8125f, 0.3125);

float ComputeVolumetric(vec3 fragPos, vec2 texCoords) {

    // We compute this in view space
    vec3 rayVector = fragPos;
    float rayLength = length(rayVector);
    vec3 rayDirection = rayVector / rayLength;
    float stepLength = rayLength / float(sampleCount);
    vec3 stepVector = rayDirection * stepLength;
 
    float foginess = 0.0f;
	
	texCoords = (0.5f * texCoords + 0.5f) * framebufferResolution;
	
	float ditherValue = ditherPattern[(int(texCoords.x) % 4) * 4 + int(texCoords.y) % 4];
	vec3 currentPosition = stepVector * ditherValue;
	
	float scatteringFactor = ComputeScattering(dot(rayDirection, light.direction));

	int cascadeIndex = 0;
	int lastCascadeIndex = 0;
	mat4 cascadeMatrix = light.shadow.cascades[0].cascadeSpace;

    for (int i = 0; i < sampleCount; i++) {
        
		float distance = -currentPosition.z;
		
		// We can leave out the zero index, because if the cascade increases, it won't ever decrease anymore
		if (distance > light.shadow.cascades[0].distance)
			cascadeIndex = 1;
		if (distance > light.shadow.cascades[1].distance)
			cascadeIndex = 2;
		if (distance > light.shadow.cascades[2].distance)
			cascadeIndex = 3;

		if (lastCascadeIndex != cascadeIndex) {
		if (cascadeIndex > 0)
			cascadeMatrix = light.shadow.cascades[1].cascadeSpace;
		if (cascadeIndex > 1)
			cascadeMatrix = light.shadow.cascades[2].cascadeSpace;
		if (cascadeIndex > 2)
			cascadeMatrix = light.shadow.cascades[3].cascadeSpace;
		}
		
		lastCascadeIndex = cascadeIndex;
		
        vec4 cascadeSpace = cascadeMatrix * vec4(currentPosition, 1.0f);
        cascadeSpace.xyz /= cascadeSpace.w;

        cascadeSpace.xyz = cascadeSpace.xyz * 0.5f + 0.5f;

        float shadowValue = texture(cascadeMaps, vec4(cascadeSpace.xy, cascadeIndex, cascadeSpace.z));
        foginess += scatteringFactor * shadowValue;

        currentPosition += stepVector;

    }

    return foginess / float(sampleCount);

}