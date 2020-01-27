layout (location = 0) out vec3 history;

layout(binding = 0) uniform sampler2D historyTexture;
layout(binding = 1) uniform sampler2D lightingTexture;
layout(binding = 2) uniform sampler2D velocityTexture;
layout(binding = 3) uniform sampler2D depthTexture;

in vec2 fTexCoord;

uniform vec2 jitter;
uniform vec2 invResolution;
uniform vec2 resolution;

// Sources for further research:
// https://de45xmedrsdbp.cloudfront.net/Resources/files/TemporalAA_small-59732822.pdf
// http://twvideo01.ubm-us.net/o1/vault/gdc2016/Presentations/Pedersen_LasseJonFuglsang_TemporalReprojectionAntiAliasing.pdf
// https://software.intel.com/en-us/articles/coarse-pixel-shading-with-temporal-supersampling
// https://github.com/playdeadgames/temporal/blob/master/Assets/Shaders/TemporalReprojection.shader
// https://community.arm.com/developer/tools-software/graphics/b/blog/posts/temporal-anti-aliasing

void main() {

    ivec2 pixel = ivec2(fTexCoord * resolution);

    vec3 neighbourhoodMin = vec3(1e9);
    vec3 neighbourhoodMax = vec3(-1e9);

    // Find best pixel in neighborhood
    ivec2 offset = ivec2(0);
    float depth = 1.0;

    vec3 lightingColor;

	// Unroll this loop?
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            ivec2 currOffset = ivec2(x, y);
            ivec2 pixelCoord = pixel + currOffset;
            vec3 color = texelFetch(lightingTexture, pixelCoord, 0).rgb;
            neighbourhoodMin = min(neighbourhoodMin, color);
            neighbourhoodMax = max(neighbourhoodMax, color);

            if (x == 0 && y == 0) {
                lightingColor = color;
            }

            float currDepth = texelFetch(depthTexture, pixelCoord, 0).r;
            if (currDepth < depth) {
                depth = currDepth;
                offset = currOffset;
            }
        }
    }

    vec2 velocity = texelFetch(velocityTexture, pixel + offset, 0).rg;
    vec2 uv = (vec2(pixel) + vec2(0.5)) * invResolution + velocity;

    vec3 historyColor = texture(historyTexture, uv).rgb;

    // Implement clipping instead of clamping and use YCoCg color space
    // There are also some weird artifacts without using neighbourhood clamping (see edges of the image)
    historyColor = clamp(historyColor, neighbourhoodMin, neighbourhoodMax);

	// Corrects subpixel sampling of history buffer
    float correction = fract(max(abs(velocity.x) * resolution.x,
		abs(velocity.y) * resolution.y)) * 0.5;

    // We need some kind of anti-flickering mechanism (luma weighted)

    float blendFactor = mix(0.05, 0.7, correction);

	// Check if we sampled outside the viewport area
    blendFactor = (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0
         || uv.y > 1.0) ? 1.0 : blendFactor;

	// We still need to reduce flickering caused by the jitter
    history = mix(historyColor, lightingColor, blendFactor);

}