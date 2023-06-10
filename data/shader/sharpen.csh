layout (local_size_x = 8, local_size_y = 8) in;

#include <common/sample.hsh>
#include <common/tiling.hsh>

layout (set = 3, binding = 0, rgba8) writeonly uniform image2D textureOut;
layout (set = 3, binding = 1) uniform sampler2D textureIn;

layout(push_constant) uniform constants {
    float sharpenFactor;
} PushConstants;

void main() {

    ivec2 size = textureSize(textureIn, 0);
    ivec2 groupOffset = GetGroupOffset2D(8);
    ivec2 coord = ivec2(gl_LocalInvocationID) + groupOffset;
    
    if (coord.x < size.x &&
        coord.y < size.y) {

        vec3 color = sampleTex(textureIn, coord, SAMPLE_CLAMP).rgb;

        vec3 up = sampleTex(textureIn, coord + ivec2(0, -1), SAMPLE_CLAMP).rgb;
        vec3 down = sampleTex(textureIn, coord + ivec2(0, 1), SAMPLE_CLAMP).rgb;
        vec3 left = sampleTex(textureIn, coord + ivec2(-1, 0), SAMPLE_CLAMP).rgb;
        vec3 right = sampleTex(textureIn, coord + ivec2(1, 0), SAMPLE_CLAMP).rgb;
        
        color = color + PushConstants.sharpenFactor * (4.0 * color - up - down - left - right);
        
        imageStore(textureOut, coord, vec4(color, 1.0));
        
    }



}