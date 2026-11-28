#include <common.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(set = 3, binding = 0, rgba16f) writeonly uniform image2D transmittanceImage;

const int transmittanceSteps = 256;

layout(set = 3, binding = 1, std140) uniform UniformBuffer {
    vec4 rayleighScatteringCoeff;
    vec4 groundAlbedo;
    float mieScatteringCoeff;
    float rayleighHeightScale;
    float mieHeightScale;
    float surfaceHeightOffset;
    float planetRadius;
    float atmosphereRadius;
} uniforms;

AtmosphereParameters GetAtmosphereParameters();
vec3 ComputeTransmittanceToTopAtmosphereBoundary(float radius, float mu,
    AtmosphereParameters atmosphere);
float GetTransmittanceSampleT(float x);

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 resolution = imageSize(transmittanceImage);

    if (pixel.x >= resolution.x || pixel.y >= resolution.y)
        return;

    vec2 uv = (vec2(pixel) + 0.5) / vec2(resolution);
    AtmosphereParameters atmosphere = GetAtmosphereParameters();

    float radius;
    float mu;
    GetRadiusMuFromTransmittanceTextureUv(uv, atmosphere, resolution, radius, mu);

    vec3 transmittance = ComputeTransmittanceToTopAtmosphereBoundary(radius, mu, atmosphere);
    imageStore(transmittanceImage, pixel, vec4(transmittance, 1.0));

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

vec3 ComputeTransmittanceToTopAtmosphereBoundary(float radius, float mu,
    AtmosphereParameters atmosphere) {

    float rayLength = DistanceToTopAtmosphereBoundary(radius, mu, atmosphere.atmosphereRadius);
    if (rayLength <= 0.0)
        return vec3(1.0);

    float opticalDepthRayleigh = 0.0;
    float opticalDepthMie = 0.0;

    float lastRayTime = 0.0;
    float lastSampleHeight = max(radius - atmosphere.planetRadius -
        atmosphere.surfaceHeightOffset, 0.0);
    float lastDensityRayleigh = GetAtmosphereDensity(lastSampleHeight, atmosphere.rayleighHeightScale);
    float lastDensityMie = GetAtmosphereDensity(lastSampleHeight, atmosphere.mieHeightScale);

    for (int i = 0; i < transmittanceSteps; i++) {
        float x1 = float(i + 1) / float(transmittanceSteps);
        float rayTime = rayLength * GetTransmittanceSampleT(x1);

        float sampleRadius = sqrt(rayTime * rayTime + 2.0 * radius * mu * rayTime + radius * radius);
        float sampleHeight = max(sampleRadius - atmosphere.planetRadius -
            atmosphere.surfaceHeightOffset, 0.0);
        float densityRayleigh = GetAtmosphereDensity(sampleHeight, atmosphere.rayleighHeightScale);
        float densityMie = GetAtmosphereDensity(sampleHeight, atmosphere.mieHeightScale);

        float stepSize = rayTime - lastRayTime;

        opticalDepthRayleigh += 0.5 * (lastDensityRayleigh + densityRayleigh) * stepSize;
        opticalDepthMie += 0.5 * (lastDensityMie + densityMie) * stepSize;

        lastRayTime = rayTime;
        lastSampleHeight = sampleHeight;
        lastDensityRayleigh = densityRayleigh;
        lastDensityMie = densityMie;
    }

    return exp(-(atmosphere.rayleighScatteringCoeff * opticalDepthRayleigh +
        atmosphere.mieScatteringCoeff * opticalDepthMie));

}

float GetTransmittanceSampleT(float x) {

    return GetAtmosphereSampleDistribution(x, 1.35);

}
