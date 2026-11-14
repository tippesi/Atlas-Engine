layout (local_size_x = 8, local_size_y = 8) in;

#define SHADOW_FILTER_1x1

#include <common.hsh>
#include <sharedUniforms.hsh>

#include <../structures.hsh>
#include <../shadow.hsh>
#include <../globals.hsh>
#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../common/stencil.hsh>
#include <../common/normalencode.hsh>
#include <../common/ign.hsh>

#include <waterBody.hsh>

layout(set = 1, binding = 12) uniform samplerCube diffuseProbe;

layout (set = 3, binding = 4) uniform sampler2D refractionTexture;
layout(set = 3, binding = 16) uniform sampler2D depthTexture;
layout(set = 3, binding = 17) uniform usampler2D stencilTexture;
layout(set = 3, binding = 18) uniform sampler2D oceanDepthTexture;
layout(set = 3, binding = 8) uniform sampler2DArrayShadow cascadeMaps;
layout(set = 3, binding = 20, rgba16f) uniform image2D outputImage;

const int shaftSampleCount = 8;
const float shaftDistance = 40.0;

// Caustic pattern at the surface, normalized to an average of one
float GetShaftPattern(vec2 position) {

    mat3 m = mat3(-2,-1,2, 3,-2,1, 1,2,2);
    vec3 a = vec3(position * 0.25, globalData.time / 4.0) * m;
    vec3 b = a * m * 0.4;
    vec3 c = b * m * 0.3;
    float pattern = min(min(length(0.5 - fract(a)), length(0.5 - fract(b))), length(0.5 - fract(c)));
    return pattern * pattern * pattern / 0.06;

}

// Average visibility of the sun along the view ray: shadows and the light focused by the waves
float GetShaftVisibility(Light light, vec3 rayOrigin, vec3 rayDirection, vec3 viewRayDirection, float rayLength,
    float waterHeight, vec3 lightDirection, float extinction, float downwellingAttenuation, float noise) {

    float marchLength = min(rayLength, shaftDistance);
    float stepLength = marchLength / float(shaftSampleCount);

    float visibility = 0.0;
    float weightSum = 0.0;
    for (int i = 0; i < shaftSampleCount; i++) {
        float t = (float(i) + noise) * stepLength;
        vec3 position = rayOrigin + rayDirection * t;
        float depth = max(waterHeight - position.y, 0.0);

        vec2 surfacePosition = position.xz - lightDirection.xz * depth / -lightDirection.y;

        // The shadow map follows the unrefracted light, so the shadow is taken where the refracted light enters
        vec3 surfaceViewPosition = vec3(globalData.vMatrix * vec4(surfacePosition.x, waterHeight, surfacePosition.y, 1.0));
        float shadow = CalculateCascadedShadow(light.shadow, cascadeMaps, surfaceViewPosition,
            vec3(0.0), vec3(0.0), 1.0);

        // Shafts build up below the surface and blur out deeper down
        float shaftStrength = saturate(depth / 3.0) * exp(-depth / 20.0);
        float shaft = mix(1.0, GetShaftPattern(surfacePosition), shaftStrength);

        // Samples which contribute more light count more
        float weight = exp(-extinction * t - downwellingAttenuation * depth / -lightDirection.y);
        visibility += weight * shadow * shaft;
        weightSum += weight;
    }

    return weightSum > 0.0 ? visibility / weightSum : 1.0;

}

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x >= imageSize(outputImage).x ||
        pixel.y >= imageSize(outputImage).y)
        return;

    Light light = LightUniforms.light;

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(imageSize(outputImage));

    float depth = textureLod(depthTexture, texCoord, 0.0).r;
    float oceanDepth = textureLod(oceanDepthTexture, texCoord, 0.0).r;

    vec3 viewSpacePos = ConvertDepthToViewSpace(depth, texCoord);
    vec3 pixelPos = vec3(globalData.ivMatrix * vec4(viewSpacePos, 1.0));
    vec3 nearPos = vec3(globalData.ivMatrix * vec4(ConvertDepthToViewSpace(0.0, texCoord), 1.0));

    int pixelsUnderWater = 0;
    int waterPixels = 0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            StencilFeatures features = DecodeStencilFeatures(texelFetch(stencilTexture, pixel + ivec2(x, y), 0).r);

            waterPixels += features.waterPixel ? 1 : 0;
            pixelsUnderWater += features.underWaterPixel ? 1 : 0;
        }
    }

    // Do majority vote, since pixel informtion gl_FrontFacing pixel information is inaccurate at a distance with shallow angle
    bool underWaterPixel = float(pixelsUnderWater) > float(waterPixels) / 2.0 ? true : false;

    StencilFeatures features = DecodeStencilFeatures(texelFetch(stencilTexture, pixel, 0).r);

    float perlinScale, shoreScaling;
    vec3 normalShoreWave;
    vec3 displacement = GetOceanDisplacement(nearPos, 0.0, perlinScale, shoreScaling, normalShoreWave);

    float waterHeight = Uniforms.translation.y + displacement.y;
    float waterDepth = waterHeight - nearPos.y;
    // With the camera under water every pixel is seen through water, also far surface pixels voted to be seen from above
    if (waterDepth < 0.0 && (depth == 1.0 && oceanDepth == 1.0 ||
        depth < oceanDepth && oceanDepth < 1.0 ||
        oceanDepth == 1.0 && pixelPos.y > Uniforms.translation.y ||
        (!underWaterPixel && features.waterPixel))) {
        return;
    }

    // Slight wobble of the image like seen through moving water
    vec2 wobble = vec2(sin(texCoord.y * 2.0 * PI * 5.0 + 5.0 * globalData.time),
        cos(texCoord.x * 2.0 * PI * 2.0 + 2.0 * globalData.time)) * 0.001;
    texCoord = clamp(texCoord + wobble, vec2(0.0), vec2(1.0));

    depth = textureLod(depthTexture, texCoord, 0.0).r;
    viewSpacePos = ConvertDepthToViewSpace(depth, texCoord);
    pixelPos = vec3(globalData.ivMatrix * vec4(viewSpacePos, 1.0));

    vec3 surfaceColor = textureLod(refractionTexture, texCoord, 0.0).rgb;

    vec3 rayOrigin = globalData.cameraLocation.xyz;
    vec3 rayDirection = normalize(pixelPos - rayOrigin);
    float rayLength = depth < 1.0 ? distance(pixelPos, rayOrigin) : 1000.0;
    // The water ends at the surface
    if (rayDirection.y > 0.0)
        rayLength = min(rayLength, max(waterHeight - rayOrigin.y, 0.0) / rayDirection.y + 1.0);

    float cameraDepth = max(waterHeight - rayOrigin.y, 0.0);
    float pixelDepth = max(Uniforms.translation.y - pixelPos.y, 0.0);

    vec3 extinction = GetWaterExtinction();
    vec3 lightDirection = GetUnderwaterLightDirection(light.direction.xyz);

    vec3 skyRadiance = textureLod(diffuseProbe, vec3(0.0, 1.0, 0.0), 0.0).rgb;
    vec3 sunScattering, ambientScattering;
    GetWaterScattering(extinction, rayDirection, cameraDepth, rayLength, light, skyRadiance,
        sunScattering, ambientScattering);

    float noise = GetInterleavedGradientNoise(vec2(pixel));
    sunScattering *= GetShaftVisibility(light, rayOrigin, rayDirection, normalize(viewSpacePos), rayLength,
        waterHeight, lightDirection, extinction.g, GetWaterDownwellingAttenuation().g, noise);

    // Surfaces were lit with the light above the water, it lost energy on the way down
    surfaceColor *= GetWaterDownwellingTransmittance(light, pixelDepth);
    vec3 color = surfaceColor * exp(-extinction * rayLength) + sunScattering + ambientScattering;

    imageStore(outputImage, pixel, vec4(color, 0.0));

}
