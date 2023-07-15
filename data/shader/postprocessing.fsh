#include <common/eotf.hsh>

layout (location = 0) out vec4 outColor;

layout (location = 0) in vec2 positionVS;

layout(set = 3, binding = 0) uniform sampler2D hdrTexture;
layout(set = 3, binding = 1) uniform sampler2D bloomFirstTexture;
layout(set = 3, binding = 2) uniform sampler2D bloomSecondTexture;
layout(set = 3, binding = 3) uniform sampler2D bloomThirdTexture;

layout(set = 3, binding = 4) uniform UniformBuffer {
    float exposure;
    float whitePoint;
    float saturation;
    float timeInMilliseconds;
    int bloomPasses;
    float aberrationStrength;
    float aberrationReversed;
    float vignetteOffset;
    float vignettePower;
    float vignetteStrength;
    vec4 vignetteColor;
} Uniforms;

const float gamma = 1.0 / 2.2;

vec3 ACESToneMap(vec3 hdrColor) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((hdrColor*(a*hdrColor+b))/
        (hdrColor*(c*hdrColor+d)+e), 0.0, 1.0);
}

vec3 ToneMap(vec3 hdrColor) {
    
    return vec3(1.0) - exp(-hdrColor);
    
}

float ToneMap(float luminance) {

    return 1.0 - exp(-luminance);

}

vec3 saturate(vec3 color, float factor) {
    const vec3 luma = vec3(0.299, 0.587, 0.114);
    vec3 pixelLuminance = vec3(dot(color, luma));
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
    vec2 uvRedChannel = (positionVS - positionVS * 0.005f * Uniforms.aberrationStrength
        * aberrationReversed) * 0.5f + 0.5f;
    vec2 uvGreenChannel = (positionVS - positionVS * 0.0025f * Uniforms.aberrationStrength) * 0.5f + 0.5f;
    vec2 uvBlueChannel =  (positionVS - positionVS * 0.005f * Uniforms.aberrationStrength
        * (1.0f - Uniforms.aberrationReversed)) * 0.5f + 0.5f;
    
    color.r = texture(hdrTexture, uvRedChannel).r;
    color.g = texture(hdrTexture, uvGreenChannel).g;
    color.b = texture(hdrTexture, uvBlueChannel).b;
    
#ifdef BLOOM
    // We want to keep a constant expression in texture[const]
    // because OpenGL ES doesn't support dynamic texture fetches
    // inside a loop
    if (Uniforms.bloomPasses > 0) {
        color.r += texture(bloomFirstTexture, uvRedChannel).r;
        color.g += texture(bloomFirstTexture, uvGreenChannel).g;
        color.b += texture(bloomFirstTexture, uvBlueChannel).b;
    }
    if (Uniforms.bloomPasses > 1) {
        color.r += texture(bloomSecondTexture, uvRedChannel).r;
        color.g += texture(bloomSecondTexture, uvGreenChannel).g;
        color.b += texture(bloomSecondTexture, uvBlueChannel).b;
    }
    if (Uniforms.bloomPasses > 2) {
        color.r += texture(bloomThirdTexture, uvRedChannel).r;
        color.g += texture(bloomThirdTexture, uvGreenChannel).g;
        color.b += texture(bloomThirdTexture, uvBlueChannel).b;
    }
#endif
#else
    color = texture(hdrTexture, texCoord).rgb;
#ifdef BLOOM
    if (Uniforms.bloomPasses > 0) {
        color += texture(bloomFirstTexture, texCoord).rgb;
    }
    if (Uniforms.bloomPasses > 1) {
        color += texture(bloomSecondTexture, texCoord).rgb;
    }
    if (Uniforms.bloomPasses > 2) {
        color += texture(bloomThirdTexture, texCoord).rgb;
    }
#endif
#endif

    color *= Uniforms.exposure;
    
    //color = color + n2rand(2.0 * fTexCoord - 1.0) / 256.0;
    
    // Apply the tone mapping because we want the colors to be back in
    // normal range
#ifdef HDR
    color = ToneMap(color);

    // Note: Tuned these two eotfs to be perceptually the same. Not sure how it turns out.
    // Haven't testet with Dolby Vision
#ifdef HYBRID_LOG_GAMMA_EOTF
    color = InverseHybridLogGammeEotf(color);
#endif

#ifdef PERCEPTUAL_QUANTIZER_EOTF
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

    color = clamp(saturate(color, Uniforms.saturation), vec3(0.0), vec3(1.0));

#ifdef VIGNETTE    
    float vignetteFactor = max(1.0 - max(pow(length(fPosition) - Uniforms.vignetteOffset,
        Uniforms.vignettePower), 0.0) * Uniforms.vignetteStrength, 0.0);
    
    color = mix(Uniforms.vignetteColor.rgb, color, Uniforms.vignetteFactor);
#endif

    outColor = vec4(color, 1.0);
    
}