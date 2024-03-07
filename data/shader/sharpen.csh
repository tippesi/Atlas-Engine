layout (local_size_x = 8, local_size_y = 8) in;

#include <common/sample.hsh>
#include <common/tiling.hsh>

layout (set = 3, binding = 0, rgba16f) writeonly uniform image2D textureOut;
layout (set = 3, binding = 1) uniform sampler2D textureIn;

layout(push_constant) uniform constants {
    float sharpenFactor;
} PushConstants;

float Luma(vec3 color) {

    const vec3 luma = vec3(0.299, 0.587, 0.114);
    return dot(color, luma);

}

vec3 Tonemap(vec3 color) {
    
    return color / (1.0 + Luma(color));
    
}

vec3 InverseTonemap(vec3 color) {
    
    return color / (1.0 - Luma(color));
    
}

void main() {

    ivec2 size = textureSize(textureIn, 0);
    ivec2 coord = ivec2(gl_GlobalInvocationID);
    
    if (coord.x < size.x &&
        coord.y < size.y) {

        vec3 color = sampleTex(textureIn, coord, SAMPLE_CLAMP).rgb;

        vec3 up = sampleTex(textureIn, coord + ivec2(0, -1), SAMPLE_CLAMP).rgb;
        vec3 down = sampleTex(textureIn, coord + ivec2(0, 1), SAMPLE_CLAMP).rgb;
        vec3 left = sampleTex(textureIn, coord + ivec2(-1, 0), SAMPLE_CLAMP).rgb;
        vec3 right = sampleTex(textureIn, coord + ivec2(1, 0), SAMPLE_CLAMP).rgb;

        // Colors are in HDR space
        up = Tonemap(up);
        down = Tonemap(down);
        left = Tonemap(left);
        right = Tonemap(right);

        color = Tonemap(color);
        
        color = color + PushConstants.sharpenFactor * (4.0 * color - up - down - left - right);
        color = InverseTonemap(color);
        
        imageStore(textureOut, coord, vec4(color, 1.0));
        
    }

}