#include <../common/utility.hsh>
#include <../common/PI.hsh>
#include <../common/stencil.hsh>
#include <../common/flatten.hsh>
#include <../common/material.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(binding = 0, rgba16f) writeonly uniform image2D resolveImage;

layout(binding = 0) uniform sampler2D historyTexture;
layout(binding = 1) uniform sampler2D currentTexture;
layout(binding = 18) uniform sampler2D roughnessMetallicAoTexture;
layout(binding = 19) uniform isampler2D offsetTexture;
layout(binding = 20) uniform usampler2D materialIdxTexture;

const ivec2 offsets[4] = ivec2[4](
    ivec2(0, 0),
    ivec2(1, 0),
    ivec2(0, 1),
    ivec2(1, 1)
);

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(resolveImage).x ||
        pixel.y > imageSize(resolveImage).y)
        return;

    int offsetIdx = texelFetch(offsetTexture, pixel, 0).r;
    ivec2 offset = offsets[offsetIdx];

    uint materialIdx = texelFetch(materialIdxTexture, pixel * 2 + offset, 0).r;
	Material material = UnpackMaterial(materialIdx);

    float roughness = texelFetch(roughnessMetallicAoTexture, pixel, 0).r;
    material.roughness *= material.roughnessMap ? roughness : 1.0;

    vec3 history = texelFetch(historyTexture, pixel, 0).rgb;
    vec3 current = texelFetch(currentTexture, pixel, 0).rgb;

    float factor = clamp(2.0 * material.roughness, 0.0, 0.95);
    vec3 resolve = mix(current, history, factor);

    imageStore(resolveImage, pixel, vec4(resolve, 0.0));

}