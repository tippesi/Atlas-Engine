// NOTE: The kernel is limited to a total size of 65 (2 * 32 + 1).
#include <common/utility.hsh>
#include <common/convert.hsh>
#include <common/normalencode.hsh>

#ifdef HORIZONTAL
layout (local_size_x = 256, local_size_y = 1) in;
#else
layout (local_size_x = 1, local_size_y = 256) in;
#endif

#ifdef BLUR_RGB
layout(set = 3, binding = 0, rgba16f) writeonly uniform image2D outputImage;
#else
layout(set = 3, binding = 0, r16f) writeonly uniform image2D outputImage;
#endif
layout(set = 3, binding = 1) uniform sampler2D inputTexture;
#ifdef DEPTH_WEIGHT
layout(set = 3, binding = 2) uniform sampler2D depthTexture;
#endif
#ifdef NORMAL_WEIGHT
layout(set = 3, binding = 3) uniform sampler2D normalTexture;
#endif

layout(push_constant) uniform constants {
    int kernelSize;
} pushConstants;

layout(set = 3, binding = 4, std140) uniform  WeightBuffer {
    vec4 data[32];
} weights;

const float normalPhi = 32.0;
const float depthPhi = 0.5;

#ifdef BLUR_RGB
shared vec3 inputs[320];
#else
shared float inputs[320];
#endif
#ifdef DEPTH_WEIGHT
shared float depths[320];
#endif
#ifdef NORMAL_WEIGHT
shared vec3 normals[320];
#endif

void LoadGroupSharedData() {

    uint dataSize = 2u * uint(pushConstants.kernelSize) + 256u;

    // Get offset of the group in image
    ivec2 offset = ivec2(gl_WorkGroupID) * ivec2(gl_WorkGroupSize);
    // Substract left extend of kernel
#ifdef HORIZONTAL
    offset.x -= pushConstants.kernelSize;
#else
    offset.y -= pushConstants.kernelSize;
#endif

    // Cooperatively load data into shared memory
    uint workGroupOffset = gl_WorkGroupSize.x * gl_WorkGroupSize.y;
    for(uint i = gl_LocalInvocationIndex; i < dataSize; i += workGroupOffset) {
        ivec2 localOffset = offset;
#ifdef HORIZONTAL
        localOffset.x += int(i);
#else
        localOffset.y += int(i);
#endif 
#ifdef BLUR_RGB
        vec3 localInput = texelFetch(inputTexture, localOffset, 0).rgb;
#else
        float localInput = texelFetch(inputTexture, localOffset, 0).r;
#endif
        inputs[i] = localInput;
#ifdef DEPTH_WEIGHT
        float localDepth = texelFetch(depthTexture, localOffset, 0).r;
        depths[i] = ConvertDepthToViewSpaceDepth(localDepth);
#endif
#ifdef NORMAL_WEIGHT
        vec3 localNormal = DecodeNormal(texelFetch(normalTexture, localOffset, 0).rg);
        normals[i] = localNormal;
#endif
    }

    barrier();
}

void main() {

    LoadGroupSharedData();

    if (gl_GlobalInvocationID.x >= imageSize(outputImage).x ||
        gl_GlobalInvocationID.y >= imageSize(outputImage).y)
        return;

    // Get offset of the group in image
    ivec2 offset = ivec2(gl_WorkGroupID) * ivec2(gl_WorkGroupSize);
    int sharedDataOffset = pushConstants.kernelSize;
    // Adds the biggest component to the shared offset
    // E.g. this is the x component for horizontal blurring
    sharedDataOffset += max3(ivec3(gl_LocalInvocationID));

#ifdef BLUR_RGB
    vec3 center, result;
#else
    float center, result;
#endif
    center = inputs[sharedDataOffset];
    result = center * weights.data[0][0];
    float totalWeight = weights.data[0][0];

#ifdef DEPTH_WEIGHT
    float centerDepth = depths[sharedDataOffset];
#endif
#ifdef NORMAL_WEIGHT
    vec3 centerNormal = normals[sharedDataOffset];
#endif

    // First sum and weight left kernel extend
    for (int i = 1; i <= pushConstants.kernelSize; i++) {
        float weight = weights.data[i / 4][i % 4];
#ifdef DEPTH_WEIGHT
        float depth = depths[sharedDataOffset - i];

        float depthDiff = abs(centerDepth - depth);
        float depthWeight = min(exp(-depthDiff / depthPhi), 1.0);
        weight *= depthWeight;
#endif
#ifdef NORMAL_WEIGHT
        vec3 normal = normals[sharedDataOffset - i];

        float normalDiff = saturate(dot(centerNormal, normal));
        float normalWeight = min(pow(normalDiff, normalPhi), 1.0);
        weight *= normalWeight;
#endif
        result += inputs[sharedDataOffset - i] * weight;
        totalWeight += weight;
    }

    // Then sum and weight right kernel extend
    for (int i = 1; i <= pushConstants.kernelSize; i++) {
        float weight = weights.data[i / 4][i % 4];
#ifdef DEPTH_WEIGHT
        float depth = depths[sharedDataOffset + i];

        float depthDiff = abs(centerDepth - depth);
        float depthWeight = min(exp(-depthDiff / depthPhi), 1.0);
        weight *= depthWeight;
#endif
#ifdef NORMAL_WEIGHT
        vec3 normal = normals[sharedDataOffset + i];

        float normalDiff = saturate(dot(centerNormal, normal));
        float normalWeight = min(pow(normalDiff, normalPhi), 1.0);
        weight *= normalWeight;
#endif
        result += inputs[sharedDataOffset + i] * weight;
        totalWeight += weight;
    }

    ivec2 pixel = offset + ivec2(gl_LocalInvocationID);

#ifdef BLUR_RGB
    imageStore(outputImage, pixel, vec4(result / totalWeight, 0.0));
#else
    imageStore(outputImage, pixel, vec4(result / totalWeight));
#endif

}