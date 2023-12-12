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

/*
Except for image variables qualified with the format qualifiers r32f, r32i, and r32ui, 
image variables must specify either memory qualifier readonly or the memory qualifier writeonly.
Reading and writing simultaneously to other formats is not supported on OpenGL ES
*/
layout (set = 3, binding = 2) uniform sampler2D inAccumImage;
layout (set = 3, binding = 3, rgba32f) writeonly uniform image2D outAccumImage;

layout(push_constant) uniform constants {
    float temporalWeight;
    float historyClipMax;
    float currentClipFactor;
    float exposure;
    int samplesPerFrame;
    float maxRadiance;
} pushConstants;

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

    vec3 historyRadiance = texelFetch(inAccumImage, pixel, 0).rgb;

    vec3 resolve = mix(currentRadiance, historyRadiance, 0.95);

    const float gamma = 1.0 / 2.2;
    vec3 color = resolve * pushConstants.exposure;
    color = vec3(1.0) - exp(-color);
    color = pow(color, vec3(gamma));

    imageStore(outAccumImage, pixel, vec4(resolve, 0.0));
    imageStore(resolveImage, pixel, vec4(color, 0.0));

}