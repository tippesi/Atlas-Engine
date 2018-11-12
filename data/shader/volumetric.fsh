#include "structures"
#include "deferred/convert"

in vec2 fTexCoord;

uniform sampler2D depthTexture;
uniform sampler2DArrayShadow cascadeMaps;
uniform Light light;
uniform int sampleCount;

out float foginess;

float ComputeVolumetric(vec3 fragPos);

void main() {

    float depth = texture(depthTexture, fTexCoord).r;

    vec3 fragPos = ConvertDepthToViewSpace(depth, fTexCoord);

    foginess = ComputeVolumetric(fragPos);

}

// Henyey-Greenstein phase function https://www.astro.umd.edu/~jph/HG_note.pdf
float ComputeScattering(float lightDotView) {
    // Range [-1;1]
    const float g = 0.0f;
    float gSquared = g * g;
    float result = 1.0f -  gSquared;
    result /= (4.0f * 3.14f * pow(1.0f + gSquared - (2.0f * g) * lightDotView, 1.5f));
    return result;
}

float ComputeVolumetric(vec3 fragPos) {

    vec4 cascadesDistance = vec4(light.shadow.cascades[0].distance,
                                 light.shadow.cascades[1].distance,
                                 light.shadow.cascades[2].distance,
                                 light.shadow.cascades[3].distance);

    vec4 cascadeCount = vec4(light.shadow.cascadeCount > 0,
                             light.shadow.cascadeCount > 1,
                             light.shadow.cascadeCount > 2,
                             light.shadow.cascadeCount > 3);

    // We compute this in view space
    vec3 rayVector = fragPos;
    float rayLength = length(rayVector);
    vec3 rayDirection = rayVector / rayLength;
    float stepLength = rayLength / sampleCount;
    vec3 stepVector = rayDirection * stepLength;
    vec3 currentPosition = vec3(0.0f);
    float foginess = 0.0f;

    for (int i = 0; i < sampleCount; i++) {
        vec4 comparison = vec4(-currentPosition.z > cascadesDistance.x,
                               -currentPosition.z > cascadesDistance.y,
                               -currentPosition.z > cascadesDistance.z,
                               -currentPosition.z > cascadesDistance.w);
        float fIndex = dot(cascadeCount, comparison);
        int index = int(fIndex);
        vec4 cascadeSpace = light.shadow.cascades[index].cascadeSpace * vec4(currentPosition, 1.0f);
        cascadeSpace.xyz /= cascadeSpace.w;

        cascadeSpace.xyz = cascadeSpace.xyz * 0.5f + 0.5f;

        float shadowValue = texture(cascadeMaps, vec4(cascadeSpace.xy, index, cascadeSpace.z));
        foginess += ComputeScattering(dot(rayDirection, light.direction)) * shadowValue;

        currentPosition += stepVector;

    }

    return (foginess / sampleCount);

}