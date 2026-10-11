#include <common.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(set = 3, binding = 0, rgba16f) writeonly uniform image2D skyViewImage;
layout(set = 3, binding = 1) uniform sampler2D transmittanceTexture;
layout(set = 3, binding = 2) uniform sampler2D multipleScatteringTexture;

layout(set = 3, binding = 3, std140) uniform UniformBuffer {
    vec4 cameraLocation;
    vec4 planetCenter;
    vec4 sunDirection;
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

const int skyViewRaySampleCount = 48;

AtmosphereParameters GetAtmosphereParameters();

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 resolution = imageSize(skyViewImage);

    if (pixel.x >= resolution.x || pixel.y >= resolution.y)
        return;

    vec2 uv = (vec2(pixel) + 0.5) / vec2(resolution);

    vec3 cameraPosition = uniforms.cameraLocation.xyz;
    vec3 planetCenter = uniforms.planetCenter.xyz;
    vec3 relativeCameraPosition = cameraPosition - planetCenter;
    vec3 cameraUp = normalize(relativeCameraPosition);
    float cameraRadius = length(relativeCameraPosition);
    AtmosphereParameters atmosphere = GetAtmosphereParameters();

    float viewZenithCosAngle;
    float lightViewCosAngle;
    GetSkyViewParamsFromTextureUv(uv, cameraRadius, atmosphere, resolution, viewZenithCosAngle,
        lightViewCosAngle);

    vec3 viewDirection = GetSkyViewDirection(viewZenithCosAngle, lightViewCosAngle,
        cameraUp, uniforms.sunDirection.xyz);

    vec3 color = IntegrateAtmosphereScattering(transmittanceTexture, multipleScatteringTexture,
        cameraPosition, viewDirection, uniforms.sunDirection.xyz, planetCenter,
        atmosphere, uniforms.sunRadiance.rgb, skyViewRaySampleCount, 1.3);

    imageStore(skyViewImage, pixel, vec4(color, 1.0));

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
