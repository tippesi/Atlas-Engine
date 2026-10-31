#include <../common/stencil.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout (set = 3, binding = 0, r8) uniform image2D reactiveMaskImage;
layout (set = 3, binding = 1) uniform usampler2D stencilTexture;
layout (set = 3, binding = 2) uniform sampler2D roughnessMetalnessAoTexture;
layout (set = 3, binding = 3) uniform usampler2D lastStencilTexture;
layout (set = 3, binding = 4) uniform sampler2D velocityTexture;

layout(push_constant) uniform constants {
    int hasHistory;
} pushConstants;

void main() {

    ivec2 size = imageSize(reactiveMaskImage);
    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    
    if (pixel.x < size.x &&
        pixel.y < size.y) {

        float reactivity = 0.0;

        StencilFeatures features = DecodeStencilFeatures(texelFetch(stencilTexture, pixel, 0).r);
        reactivity = features.responsivePixel ? 0.8 : reactivity;

        vec3 roughnessMetallicAo = texelFetch(roughnessMetalnessAoTexture, pixel, 0).rgb;
        float reflectiveReactivity = min(1.0, 5.0 * roughnessMetallicAo.r);
        reactivity = max(reactivity, mix(0.5, 0.0, reflectiveReactivity));

        bool rejectHistory = features.noTemporalAccumulation;
        if (pushConstants.hasHistory > 0) {
            vec2 uv = (vec2(pixel) + 0.5) / vec2(size) + texelFetch(velocityTexture, pixel, 0).rg;
            ivec2 historyPixel = clamp(ivec2(uv * vec2(size)), ivec2(0), size - 1);
            StencilFeatures historyFeatures = DecodeStencilFeatures(texelFetch(lastStencilTexture, historyPixel, 0).r);
            StencilFeatures previousFeatures = DecodeStencilFeatures(texelFetch(lastStencilTexture, pixel, 0).r);
            rejectHistory = rejectHistory || historyFeatures.noTemporalAccumulation || previousFeatures.noTemporalAccumulation;
        }
        reactivity = rejectHistory ? 1.0 : reactivity;

        imageStore(reactiveMaskImage, pixel, vec4(reactivity, 0.0, 0.0, 0.0));
        
    }

}