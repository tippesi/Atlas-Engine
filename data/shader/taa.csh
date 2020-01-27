#include <common/indexing>
#include <common/sample>

layout (local_size_x = 8, local_size_y = 8) in;

layout (binding = 0, rgba16f) writeonly uniform image2D historyOut;

layout(binding = 1) uniform sampler2D historyTexture;
layout(binding = 2) uniform sampler2D lightingTexture;
layout(binding = 3) uniform sampler2D velocityTexture;
layout(binding = 4) uniform sampler2D depthTexture;

uniform vec2 jitter;
uniform vec2 invResolution;
uniform vec2 resolution;

void main() {

    const ivec2 coord = ivec2(gl_GlobalInvocationID.xy);

    if (coord.x < int(resolution.x) && 
        coord.y < int(resolution.y)) {

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
                ivec2 pixelCoord = coord + currOffset;
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

        vec2 velocity = texelFetch(velocityTexture, 
            coord + offset, 0).rg;
        vec2 uv = (vec2(coord)) * invResolution + velocity;

        vec3 historyColor = sampleTexBilinear(historyTexture, uv,
            SAMPLE_CLAMP).rgb;

        historyColor = clamp(historyColor, neighbourhoodMin, neighbourhoodMax);

        // Corrects subpixel sampling of history buffer
        float correction = fract(max(abs(velocity.x) * resolution.x,
            abs(velocity.y) * resolution.y)) * 0.5;

        float blendFactor = mix(0.1, 0.8, correction);

        // Check if we sampled outside the viewport area
        blendFactor = (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0
            || uv.y > 1.0) ? 1.0 : blendFactor;

        // We still need to reduce flickering caused by the jitter
        vec3 history = mix(historyColor, lightingColor, blendFactor);

        imageStore(historyOut, coord, vec4(history, 1.0));

    }

}