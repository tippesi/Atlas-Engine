#include <globals.hsh>
#include <common/eotf.hsh>
#include <common/random.hsh>

layout (location = 0) out vec4 outColor;

layout (location = 0) in vec2 positionVS;

layout(set = 3, binding = 0) uniform sampler2D hdrTexture;
layout(set = 3, binding = 1) uniform sampler2D bloomTexture;

layout(set = 3, binding = 4) uniform UniformBuffer {
    float exposure;
    float paperWhiteLuminance;
    float maxScreenLuminance;
    float saturation;
    float contrast;
    float filmGrainStrength;
    float bloomStrength;
    float aberrationStrength;
    float aberrationReversed;
    float vignetteOffset;
    float vignettePower;
    float vignetteStrength;
    vec4 vignetteColor;
    vec4 tintColor;
} Uniforms;

const float gamma = 1.0 / 2.2;
float screenMaxNits = Uniforms.maxScreenLuminance;
float paperWhiteNits = Uniforms.paperWhiteLuminance;

vec3 ACESToneMap(vec3 hdrColor) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((hdrColor*(a*hdrColor+b))/
        (hdrColor*(c*hdrColor+d)+e), 0.0, 1.0);
}

vec3 ACESFilmRec2020(vec3 hdrColor)  {
    float a = 15.8;
    float b = 2.12;
    float c = 1.2;
    float d = 5.92;
    float e = 1.9;
    return (hdrColor * (a * hdrColor + b)) / (hdrColor * (c * hdrColor + d) + e);
}

vec3 ToneMap(vec3 hdrColor) {
    
    return vec3(1.0) - exp(-hdrColor);
    
}

float ToneMap(float luminance) {

    return 1.0 - exp(-luminance);

}

vec3 saturate(vec3 color, float factor) {
    const vec3 luma = vec3(0.299, 0.587, 0.114);
    vec3 pixelLuminance = max(vec3(dot(color, luma)), vec3(0.0));
    return mix(pixelLuminance, color, factor);
}

const mat3 RGBToYCoCgMatrix = mat3(0.25, 0.5, -0.25, 0.5, 0.0, 0.5, 0.25, -0.5, -0.25);
const mat3 YCoCgToRGBMatrix = mat3(1.0, 1.0, 1.0, 1.0, 0.0, -1.0, -1.0, 1.0, -1.0);

vec3 RGBToYCoCg(vec3 RGB) {

    return RGBToYCoCgMatrix * RGB;

}

vec3 YCoCgToRGB(vec3 YCoCg) {

    return YCoCgToRGBMatrix * YCoCg;

}

void main() {
    
    vec2 texCoord = 0.5 * positionVS + 0.5;
    vec3 color = vec3(0.0);
    
#ifdef CHROMATIC_ABERRATION
    vec2 uvRedChannel = (positionVS - positionVS * 0.005 * Uniforms.aberrationStrength
        * Uniforms.aberrationReversed) * 0.5 + 0.5;
    vec2 uvGreenChannel = (positionVS - positionVS * 0.0025 * Uniforms.aberrationStrength) * 0.5 + 0.5;
    vec2 uvBlueChannel =  (positionVS - positionVS * 0.005 * Uniforms.aberrationStrength
        * (1.0f - Uniforms.aberrationReversed)) * 0.5 + 0.5;
    
    color.r = textureLod(hdrTexture, uvRedChannel, 0.0).r;
    color.g = textureLod(hdrTexture, uvGreenChannel, 0.0).g;
    color.b = textureLod(hdrTexture, uvBlueChannel, 0.0).b;
    
#ifdef BLOOM
    color.r += Uniforms.bloomStrength * textureLod(bloomTexture, uvRedChannel, 0.0).r;
    color.g += Uniforms.bloomStrength * textureLod(bloomTexture, uvGreenChannel, 0.0).g;
    color.b += Uniforms.bloomStrength * textureLod(bloomTexture, uvBlueChannel, 0.0).b;
#endif
#else
    color = textureLod(hdrTexture, texCoord, 0.0).rgb;
#ifdef BLOOM
    color += Uniforms.bloomStrength * textureLod(bloomTexture, texCoord, 0.0).rgb;
#endif
#endif

    color *= Uniforms.exposure;

#ifdef FILM_GRAIN
    color = color + color * Uniforms.filmGrainStrength * (2.0 * random(vec3(texCoord * 1000.0, globalData.time)) - 1.0);
    color = max(color, vec3(0.0));
#endif
    
    // Apply the tone mapping because we want the colors to be back in
    // normal range
#ifdef HDR
    // Interesting approach to make bright parts look more white using luma: https://github.com/libretro/RetroArch/blob/14ce660a38f99d448b32ed752ddaf1f250dcf669/gfx/drivers/d3d_shaders/hdr_sm5.hlsl.h
    
    // Note: Tuned these two eotfs to be perceptually the same. Not sure how it turns out.
    // Haven't tested with Dolby Vision
#ifdef HYBRID_LOG_GAMMA_EOTF
    // Dark regions are getting crushed too much, correct for that
    color = pow(color, vec3(0.9));
    color = Rec709ToRec2020(color);
    // Made for 1000nits max brightness, so scale by this factor
    color = ACESFilmRec2020(color) * screenMaxNits / 1000.0;

    color.rgb = InverseHybridLogGammeEotf(color);
#endif

#ifdef PERCEPTUAL_QUANTIZER_EOTF
    color = Rec709ToRec2020(color);
    // Made for 1000nits max brightness, so scale by this factor
    color = ACESFilmRec2020(color) * screenMaxNits / 1000.0;

    color *= paperWhiteNits / 10000.0;
    color = InversePerceptualQuantizerEotf(color);
#endif
    
#else
#ifdef FILMIC_TONEMAPPING
    color = ACESToneMap(color);
#else
    color = ToneMap(color);
#endif

#ifdef GAMMA_CORRECTION
    color = pow(color, vec3(gamma));
#endif
#endif

    color = color * Uniforms.tintColor.rgb;

    color = clamp(saturate(color, Uniforms.saturation), vec3(0.0), vec3(1.0));

    color = ((color - 0.5) * max(Uniforms.contrast, 0.0)) + 0.5;

#ifdef VIGNETTE    
    float vignetteFactor = max(1.0 - max(pow(length(fPosition) - Uniforms.vignetteOffset,
        Uniforms.vignettePower), 0.0) * Uniforms.vignetteStrength, 0.0);
    
    color = mix(Uniforms.vignetteColor.rgb, color, Uniforms.vignetteFactor);
#endif
    outColor = vec4(color, 1.0);
    
}