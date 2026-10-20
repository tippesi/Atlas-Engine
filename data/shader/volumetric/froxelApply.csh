layout (local_size_x = 8, local_size_y = 8) in;

#include <../globals.hsh>
#include <../common/convert.hsh>
#include <../common/stencil.hsh>

#include <froxel.hsh>

layout(set = 3, binding = 0, rgba16f) writeonly uniform image2D volumetricImage;
layout(set = 3, binding = 1) uniform sampler2D depthTexture;
layout(set = 3, binding = 3) uniform sampler2D oceanDepthTexture;
layout(set = 3, binding = 4) uniform usampler2D oceanStencilTexture;
layout(set = 3, binding = 20) uniform sampler3D integratedFroxelTexture;

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 resolution = imageSize(volumetricImage);
    if (any(greaterThanEqual(pixel, resolution)))
        return;

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(resolution);

    float depth = textureLod(depthTexture, texCoord, 0.0).r;
    vec3 endPos = ConvertDepthToViewSpace(depth, texCoord);

#ifdef OCEAN
    StencilFeatures features = DecodeStencilFeatures(textureLod(oceanStencilTexture, texCoord, 0.0).r);

    float oceanDepth = textureLod(oceanDepthTexture, texCoord, 0.0).r;
    vec3 oceanPos = ConvertDepthToViewSpace(oceanDepth, texCoord);

    if (oceanDepth < depth && !features.underWaterPixel) {
        endPos = oceanPos;
    }
#endif

    // Each slice stores the integral up to its far end, inside the first slice it is interpolated from the camera
    float slice = GetFroxelSlice(-endPos.z);
    vec2 unjitteredTexCoord = texCoord - 0.5 * globalData.jitterCurrent;
    vec2 gridTexCoord = unjitteredTexCoord * vec2(resolution) / vec2(pushConstants.columnCount * pushConstants.tileSize);
    float sliceTexCoord = (max(slice - 1.0, 0.0) + 0.5) / float(pushConstants.sliceCount);
    vec4 integrated = textureLod(integratedFroxelTexture, vec3(gridTexCoord, sliceTexCoord), 0.0);

    float firstSliceWeight = clamp(slice, 0.0, 1.0);
    integrated = mix(vec4(0.0, 0.0, 0.0, 1.0), integrated, firstSliceWeight);

    imageStore(volumetricImage, pixel, integrated);

}
