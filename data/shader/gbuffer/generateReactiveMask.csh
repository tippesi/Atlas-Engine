#include <../common/stencil.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout (set = 3, binding = 0, r8) uniform image2D reactiveMaskImage;
layout (set = 3, binding = 1) uniform usampler2D stencilTexture;

void main() {

    ivec2 size = imageSize(reactiveMaskImage);
    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    
    if (pixel.x < size.x &&
        pixel.y < size.y) {

        float reactivity = 0.0;

        StencilFeatures features = DecodeStencilFeatures(texelFetch(stencilTexture, pixel, 0).r);
        reactivity = features.responsivePixel ? 0.5 : reactivity;

        imageStore(reactiveMaskImage, pixel, vec4(reactivity, 0.0, 0.0, 0.0));
        
    }

}