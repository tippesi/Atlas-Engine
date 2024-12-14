// Based on https://bruop.github.io/exposure/

#include <exposure.hsh>

layout (local_size_x = 16, local_size_x = 16) in;

layout(set = 3, binding = 0) uniform sampler2D hdrTexture;

layout(push_constant) uniform constants {
    float logLuminanceMin;
    float invLogLuminanceRange;
} pushConstants;

shared uint sharedHistogram[histogramBinCount];

uint CalculateBin(vec3 color, float logLuminanceMin, float invLogLuminanceRange) {

    float luma = Luma(color);
    if (luma < lumaEpsilon)
        return 0u;

    float logLuma = clamp((log2(luma) - logLuminanceMin) * invLogLuminanceRange, 0.0, 1.0);

    return uint(logLuma * 254.0 + 1.0);

}

void main() {

    sharedHistogram[gl_LocalInvocationIndex] = 0u;

    barrier();

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x < textureSize(hdrTexture, 0).x &&
        pixel.y < textureSize(hdrTexture, 0).y) {

        vec3 color = texelFetch(hdrTexture, pixel, 0).rgb;
        uint binIdx = CalculateBin(color, pushConstants.logLuminanceMin, 
            pushConstants.invLogLuminanceRange);

        atomicAdd(sharedHistogram[binIdx], 1u);

    }

    barrier();

    atomicAdd(histogram[gl_LocalInvocationIndex], sharedHistogram[gl_LocalInvocationIndex]);

}