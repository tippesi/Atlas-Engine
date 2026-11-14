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

// Cubic B-spline in x and y from four linear samples, see "Fast Third-Order Texture Filtering", Sigg and Hadwiger 2005
vec4 SampleIntegratedFroxels(vec3 texCoord) {

    vec2 size = vec2(pushConstants.columnCount);
    vec2 coord = texCoord.xy * size - 0.5;
    vec2 index = floor(coord);
    vec2 fraction = coord - index;
    vec2 oneMinusFraction = 1.0 - fraction;

    vec2 w0 = oneMinusFraction * oneMinusFraction * oneMinusFraction / 6.0;
    vec2 w1 = (4.0 - 6.0 * fraction * fraction + 3.0 * fraction * fraction * fraction) / 6.0;
    vec2 w3 = fraction * fraction * fraction / 6.0;
    vec2 w2 = 1.0 - w0 - w1 - w3;

    vec2 g0 = w0 + w1;
    vec2 g1 = w2 + w3;
    vec2 coord0 = (index - 0.5 + w1 / g0) / size;
    vec2 coord1 = (index + 1.5 + w3 / g1) / size;

    vec4 sample00 = textureLod(integratedFroxelTexture, vec3(coord0.x, coord0.y, texCoord.z), 0.0);
    vec4 sample10 = textureLod(integratedFroxelTexture, vec3(coord1.x, coord0.y, texCoord.z), 0.0);
    vec4 sample01 = textureLod(integratedFroxelTexture, vec3(coord0.x, coord1.y, texCoord.z), 0.0);
    vec4 sample11 = textureLod(integratedFroxelTexture, vec3(coord1.x, coord1.y, texCoord.z), 0.0);

    return g0.y * (g0.x * sample00 + g1.x * sample10) + g1.y * (g0.x * sample01 + g1.x * sample11);

}

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
    vec4 integrated = SampleIntegratedFroxels(vec3(gridTexCoord, sliceTexCoord));

    float firstSliceWeight = clamp(slice, 0.0, 1.0);
    integrated = mix(vec4(0.0, 0.0, 0.0, 1.0), integrated, firstSliceWeight);

    imageStore(volumetricImage, pixel, integrated);

}
