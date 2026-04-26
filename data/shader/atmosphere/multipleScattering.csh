#include <common.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(set = 3, binding = 0, rgba16f) writeonly uniform image2D multipleScatteringImage;
layout(set = 3, binding = 1) uniform sampler2D transmittanceTexture;

layout(set = 3, binding = 2, std140) uniform UniformBuffer {
    vec4 sunRadiance;
    vec4 rayleighScatteringCoeff;
    vec4 groundAlbedo;
    float mieScatteringCoeff;
    float rayleighHeightScale;
    float mieHeightScale;
    float surfaceHeightOffset;
    float planetRadius;
    float atmosphereRadius;
} uniforms;

const int multipleScatteringDirectionSampleCount = 64;
const int multipleScatteringRaySampleCount = 20;

AtmosphereParameters GetAtmosphereParameters();
vec3 GetSphericalFibonacciDirection(int index, int count);

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 resolution = imageSize(multipleScatteringImage);

    if (pixel.x >= resolution.x || pixel.y >= resolution.y)
        return;

    vec2 uv = (vec2(pixel) + 0.5) / vec2(resolution);

    float radius;
    float muS;
    AtmosphereParameters atmosphere = GetAtmosphereParameters();
    GetRadiusMuFromMultipleScatteringTextureUv(uv, atmosphere, resolution, radius, muS);

    vec3 samplePosition = vec3(0.0, 0.0, radius);
    vec3 sunDirection = vec3(sqrt(max(1.0 - muS * muS, 0.0)), 0.0, muS);

    vec3 inScatteredLuminance = vec3(0.0);
    vec3 multiScatteringFactor = vec3(0.0);

    float sampleWeight = 4.0 * PI / float(multipleScatteringDirectionSampleCount);

    for (int i = 0; i < multipleScatteringDirectionSampleCount; i++) {
        vec3 rayDirection = GetSphericalFibonacciDirection(i, multipleScatteringDirectionSampleCount);

        vec3 rayLuminance;
        vec3 rayMultiScatteringFactor;
        IntegrateAtmosphereMultiScattering(transmittanceTexture, samplePosition, rayDirection, sunDirection,
            vec3(0.0), atmosphere, uniforms.sunRadiance.rgb, multipleScatteringRaySampleCount, 1.4,
            rayLuminance, rayMultiScatteringFactor);

        inScatteredLuminance += rayLuminance * sampleWeight * atmosphereIsotropicPhase;
        multiScatteringFactor += rayMultiScatteringFactor * sampleWeight;
    }

    vec3 clampedFactor = min(multiScatteringFactor, vec3(0.999));
    vec3 multipleScattering = inScatteredLuminance / max(vec3(1.0) - clampedFactor, vec3(0.01));

    imageStore(multipleScatteringImage, pixel, vec4(multipleScattering, 1.0));

}

AtmosphereParameters GetAtmosphereParameters() {

    return AtmosphereParameters(
        uniforms.rayleighScatteringCoeff.rgb,
        uniforms.groundAlbedo.rgb,
        uniforms.mieScatteringCoeff,
        uniforms.rayleighHeightScale,
        uniforms.mieHeightScale,
        uniforms.surfaceHeightOffset,
        uniforms.planetRadius,
        uniforms.atmosphereRadius
    );

}

vec3 GetSphericalFibonacciDirection(int index, int count) {

    float i = float(index);
    float n = float(count);
    float phi = 2.0 * PI * fract(i * 0.6180339887498948);
    float cosTheta = 1.0 - (2.0 * i + 1.0) / n;
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

}
