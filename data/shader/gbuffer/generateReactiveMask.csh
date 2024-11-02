#include <../common/stencil.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout (set = 3, binding = 0, r8) uniform image2D reactiveMaskImage;
layout (set = 3, binding = 1) uniform usampler2D stencilTexture;
layout (set = 3, binding = 2) uniform sampler2D roughnessMetalnessAoTexture;

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
        //reactivity = max(reactivity, mix(1.0, 0.0, reflectiveReactivity));

        imageStore(reactiveMaskImage, pixel, vec4(reactivity, 0.0, 0.0, 0.0));
        
    }

}