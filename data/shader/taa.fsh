layout (location = 0) out vec3 history;

layout(binding = 0) uniform sampler2D historyTexture;
layout(binding = 1) uniform sampler2D lightingTexture;
layout(binding = 2) uniform sampler2D velocityTexture;
layout(binding = 3) uniform sampler2D depthTexture;

in vec2 fTexCoord;

uniform vec2 jitter;
uniform vec2 invResolution;
uniform vec2 resolution;

void main() {

    vec3 neighbourhoodMin = vec3(1e9);
    vec3 neighbourhoodMax = vec3(-1e9);

    // Find best pixel in neighborhood
    vec2 offset = vec2(0.0);
    float depth = 1.0;

	// Unroll this loop?
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec2 currOffset = vec2(float(x), float(y)) * invResolution;
            vec2 pixelCoord = fTexCoord + currOffset;
            vec3 color = texture(lightingTexture, pixelCoord).rgb;
            neighbourhoodMin = min(neighbourhoodMin, color);
            neighbourhoodMax = max(neighbourhoodMax, color);

            float currDepth = texture(depthTexture, pixelCoord).r;
            if (currDepth < depth) {
                depth = currDepth;
                offset = currOffset;
            }
        }
    }

    vec2 velocity = texture(velocityTexture, fTexCoord).rg;
    vec2 uv = fTexCoord + velocity;

    vec3 historyColor = texture(historyTexture, uv).rgb;
    vec3 lightingColor = texture(lightingTexture, fTexCoord).rgb;

    historyColor = clamp(historyColor, neighbourhoodMin, neighbourhoodMax);

	// Corrects subpixel sampling of history buffer
    float correction = fract(max(abs(velocity.x) * resolution.x,
		abs(velocity.y) * resolution.y)) * 0.5;

    float blendFactor = mix(0.1, 0.7, correction);

	// Check if we sampled outside the viewport area
    blendFactor = (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0
         || uv.y > 1.0) ? 1.0 : blendFactor;

	// We still need to reduce flickering caused by the jitter
    history = mix(historyColor, lightingColor, blendFactor);

}