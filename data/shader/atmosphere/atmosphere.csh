#include <../globals.hsh>
#include <common.hsh>
#include <../common/convert.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

#ifndef ENVIRONMENT_PROBE
layout(set = 3, binding = 0, rgba16f) writeonly uniform image2D colorImage;
layout(set = 3, binding = 1, rg16f) writeonly uniform image2D velocityImage;
layout(set = 3, binding = 2) uniform sampler2D depthTexture;
#else
layout(set = 3, binding = 0, rgba16f) writeonly uniform imageCube colorImage;
#endif

layout(set = 3, binding = 3, std140) uniform UniformBuffer {
    mat4 ivMatrix;
    mat4 ipMatrix;
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

#ifndef ENVIRONMENT_PROBE
layout(set = 3, binding = 4) uniform sampler2D skyViewTexture;
layout(set = 3, binding = 5) uniform sampler2D transmittanceTexture;
#else
layout(set = 3, binding = 4) uniform sampler2D transmittanceTexture;
layout(set = 3, binding = 5) uniform sampler2D multipleScatteringTexture;
layout(set = 3, binding = 6, std140) uniform MatricesBuffer {
    mat4 data[6];
} matrices;
#endif

#ifdef ENVIRONMENT_PROBE
const int probeRaySampleCount = 48;
#endif

vec3 GetSunDisk(vec3 viewDirection, AtmosphereParameters atmosphere);

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 resolution = imageSize(colorImage);

    if (pixel.x >= resolution.x || pixel.y >= resolution.y)
        return;

#ifndef ENVIRONMENT_PROBE
    float depth = texelFetch(depthTexture, pixel, 0).r;
    if (depth < 1.0)
        return;
#else
    float depth = 1.0;
#endif

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(resolution);

    vec3 viewPosition = ConvertDepthToViewSpace(depth, texCoord, uniforms.ipMatrix);
#ifndef ENVIRONMENT_PROBE
    vec3 worldPosition = vec3(uniforms.ivMatrix * vec4(viewPosition, 1.0));
#else
    vec3 worldPosition = vec3(matrices.data[gl_GlobalInvocationID.z] * vec4(viewPosition, 1.0));
#endif

    vec3 viewDirection = normalize(worldPosition - uniforms.cameraLocation.xyz);
    AtmosphereParameters atmosphere = AtmosphereParameters(
        uniforms.rayleighScatteringCoeff.rgb,
        uniforms.groundAlbedo.rgb,
        uniforms.mieScatteringCoeff,
        uniforms.rayleighHeightScale,
        uniforms.mieHeightScale,
        uniforms.surfaceHeightOffset,
        uniforms.planetRadius,
        uniforms.atmosphereRadius
    );

#ifndef ENVIRONMENT_PROBE
    vec3 color = GetSkyView(skyViewTexture, viewDirection, uniforms.cameraLocation.xyz,
        uniforms.planetCenter.xyz, uniforms.sunDirection.xyz, atmosphere);
#else
    vec3 color = IntegrateAtmosphereScattering(transmittanceTexture, multipleScatteringTexture,
        uniforms.cameraLocation.xyz, viewDirection, uniforms.sunDirection.xyz, uniforms.planetCenter.xyz,
        atmosphere, uniforms.sunRadiance.rgb, probeRaySampleCount, 1.35);
#endif

    color += GetSunDisk(viewDirection, atmosphere);

#ifndef ENVIRONMENT_PROBE
    vec3 ndcCurrent = (globalData.pMatrix * vec4(viewPosition, 1.0)).xyw;
    vec3 ndcLast = (globalData.pvMatrixLast * vec4(worldPosition, 1.0)).xyw;

    vec2 ndcLastPosition = ndcLast.xy / ndcLast.z;
    vec2 ndcCurrentPosition = ndcCurrent.xy / ndcCurrent.z;

    ndcLastPosition -= globalData.jitterLast;
    ndcCurrentPosition -= globalData.jitterCurrent;

    vec2 velocity = (ndcLastPosition - ndcCurrentPosition) * 0.5;

    imageStore(velocityImage, pixel, vec4(velocity, 0.0, 1.0));
    imageStore(colorImage, pixel, vec4(color, 1.0));
#else
    imageStore(colorImage, ivec3(pixel, int(gl_GlobalInvocationID.z)), vec4(color, 1.0));
#endif

}

vec3 GetSunDisk(vec3 viewDirection, AtmosphereParameters atmosphere) {

    float viewSunCosAngle = dot(viewDirection, uniforms.sunDirection.xyz);
    float sunDisk = smoothstep(cos(atmosphereSunAngularRadius * 1.25), cos(atmosphereSunAngularRadius),
        viewSunCosAngle);

    vec3 cameraUp = normalize(uniforms.cameraLocation.xyz - uniforms.planetCenter.xyz);
    float cameraRadius = length(uniforms.cameraLocation.xyz - uniforms.planetCenter.xyz);
    float viewZenithCosAngle = dot(viewDirection, cameraUp);
    vec3 transmittance = GetTransmittanceToTopAtmosphereBoundary(transmittanceTexture, cameraRadius,
        viewZenithCosAngle, atmosphere);

    return transmittance * uniforms.sunRadiance.rgb * sunDisk * 5.0;

}
