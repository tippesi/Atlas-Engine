#include <../raytracer/deferredShadow.hsh>

layout (local_size_x = 8, local_size_y = 4) in;

layout (set = 3, binding = 0, rgba16f) uniform image2D rtrImage;

layout(push_constant) uniform constants {
    ivec2 resolution;
    int sampleCount;
    int lightSampleCount;
} pushConstants;

void main() {

    ivec2 resolution = pushConstants.resolution;
    if (int(gl_GlobalInvocationID.x) >= resolution.x || int(gl_GlobalInvocationID.y) >= resolution.y)
        return;

    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    int pixelIdx = pixel.y * resolution.x + pixel.x;

    // Pixels which weren't ray traced, e.g. because of an SSR hit, keep their reflection
    if (deferredSamples[pixelIdx * pushConstants.sampleCount].radiance.w < 0.0)
        return;

    vec4 reflection = imageLoad(rtrImage, pixel);
    for (int i = 0; i < pushConstants.sampleCount; i++) {
        int sampleIdx = pixelIdx * pushConstants.sampleCount + i;
        if (deferredSamples[sampleIdx].radiance.w == 0.0)
            continue;

        vec3 radiance = deferredSamples[sampleIdx].radiance.rgb;
        for (int j = 0; j < pushConstants.lightSampleCount; j++) {
            vec4 light = deferredLights[sampleIdx * pushConstants.lightSampleCount + j];
            radiance += light.rgb * light.w;
        }

        float radianceLimit = deferredSamples[sampleIdx].lobeWeight.w;
        float radianceMax = max(max(max(radiance.r,
            max(radiance.g, radiance.b)), radianceLimit), 1e-12);
        reflection.rgb += radiance * (radianceLimit / radianceMax);
    }

    reflection.rgb /= float(pushConstants.sampleCount);
    imageStore(rtrImage, pixel, reflection);

}
