#include <../common/utility.hsh>
#include <../common/convert.hsh>
#include <../common/PI.hsh>
#include <../common/stencil.hsh>
#include <../common/flatten.hsh>
#include <../common/material.hsh>
#include <../common/random.hsh>
#include <../common/normalencode.hsh>

layout (local_size_x = 16, local_size_y = 16) in;

layout (set = 3, binding = 0, rgba8) writeonly uniform image2D resolveImage;
layout (set = 3, binding = 1, r32ui) readonly uniform uimage2DArray frameAccumImage;
layout (set = 3, binding = 2) uniform sampler2D inAccumImage;
layout (set = 3, binding = 3, rgba32f) writeonly uniform image2D outAccumImage;
layout (set = 3, binding = 4) uniform sampler2D velocityTexture;

layout(push_constant) uniform constants {
    float temporalWeight;
    float historyClipMax;
    float currentClipFactor;
    float exposure;
    int samplesPerFrame;
    float maxRadiance;
} pushConstants;

vec2 invResolution = 1.0 / vec2(imageSize(resolveImage));
vec2 resolution = vec2(imageSize(resolveImage));

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(resolveImage).x ||
        pixel.y > imageSize(resolveImage).y)
        return;

    uvec3 frameAccumData;
    frameAccumData.r = imageLoad(frameAccumImage, ivec3(pixel, 0)).r;
    frameAccumData.g = imageLoad(frameAccumImage, ivec3(pixel, 1)).r;
    frameAccumData.b = imageLoad(frameAccumImage, ivec3(pixel, 2)).r;

    uint maxValuePerSample = 0xFFFFFFFF / uint(pushConstants.samplesPerFrame);
    vec3 currentRadiance = vec3(vec3(frameAccumData) / float(maxValuePerSample))  / float(pushConstants.samplesPerFrame) * pushConstants.maxRadiance;

    vec2 velocity = texelFetch(velocityTexture, pixel, 0).rg;

    vec2 historyUV = (vec2(pixel) + vec2(0.5)) * invResolution + velocity;
    vec2 historyPixel = vec2(pixel) + velocity * resolution;

    vec4 history = textureLod(inAccumImage, historyUV, 0);
    vec3 historyRadiance = history.rgb;
    float historyLength = history.a;

    float factor = 0.95;
    factor = (historyUV.x < 0.0 || historyUV.y < 0.0 || historyUV.x > 1.0
         || historyUV.y > 1.0) ? 0.0 : factor;

    if (factor < 0.1) {
        historyLength = 0.0;
    }

    factor = min(factor, historyLength / (historyLength + 1.0));

    vec3 resolve = mix(currentRadiance, historyRadiance, factor);

    const float gamma = 1.0 / 2.2;
    vec3 color = resolve * pushConstants.exposure;
    color = vec3(1.0) - exp(-color);
    color = pow(color, vec3(gamma));

    imageStore(outAccumImage, pixel, vec4(resolve, historyLength + 1.0));
    imageStore(resolveImage, pixel, vec4(color, 0.0));

}