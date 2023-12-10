#include <common/utility.hsh>
#include <common/PI.hsh>
#include <common/stencil.hsh>
#include <common/flatten.hsh>
#include <common/normalencode.hsh>
#include <common/normalreconstruction.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(set = 3, binding = 0, rg16f) writeonly uniform image2D normalImage;

layout(set = 3, binding = 1) uniform sampler2D depthTexture;

const uint sharedDataSize = (gl_WorkGroupSize.x + 2) * (gl_WorkGroupSize.y + 2);
const ivec2 unflattenedSharedDataSize = ivec2(gl_WorkGroupSize) + ivec2(2);

shared float depths[sharedDataSize];

void LoadGroupSharedData() {

    ivec2 resolution = imageSize(normalImage);
    ivec2 workGroupOffset = ivec2(gl_WorkGroupID) * ivec2(gl_WorkGroupSize) - ivec2(1);

    uint workGroupSize = gl_WorkGroupSize.x * gl_WorkGroupSize.y;
    for(uint i = gl_LocalInvocationIndex; i < sharedDataSize; i += workGroupSize) {
        ivec2 localOffset = Unflatten2D(int(i), unflattenedSharedDataSize);
        ivec2 texel = localOffset + workGroupOffset;

        texel = clamp(texel, ivec2(0), ivec2(resolution) - ivec2(1));

        depths[i] = texelFetch(depthTexture, texel, 0).r;
    }

    barrier();

}

float GetPixelDepth(ivec2 pixelOffset) {

    ivec2 pixel = ivec2(gl_LocalInvocationID) + ivec2(1);
    int sharedMemoryIdx = Flatten2D(pixel + pixelOffset, unflattenedSharedDataSize);
    return depths[sharedMemoryIdx];

}

void main() {

    LoadGroupSharedData();
    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(normalImage).x ||
        pixel.y > imageSize(normalImage).y)
        return;

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(imageSize(normalImage));

    float depthCenter = GetPixelDepth(ivec2(0, 0));
    float depthLeft = GetPixelDepth(ivec2(-1, 0));
    float depthRight = GetPixelDepth(ivec2(1, 0));
    float depthTop = GetPixelDepth(ivec2(0, -1));
    float depthBottom = GetPixelDepth(ivec2(0, 1));

    ivec2 resolution = ivec2(imageSize(normalImage));
    vec3 normal = ReconstructNormal(resolution, depthCenter, depthLeft, depthRight,
        depthTop, depthBottom, texCoord);

    vec2 encoded = EncodeNormal(normal);
    imageStore(normalImage, pixel, vec4(encoded, 0.0, 0.0));

}