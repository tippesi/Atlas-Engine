#include <../deferred/deferred.hsh>

#include <../brdf/brdfEval.hsh>
#include <../common/flatten.hsh>

#ifdef DDGI
#include <../ddgi/ddgi.hsh>
#endif

layout (local_size_x = 8, local_size_y = 8) in;

layout(set = 3, binding = 0, rgba16f) writeonly uniform image2D image;

layout(set = 3, binding = 1) uniform sampler2D lowResTexture;
layout(set = 3, binding = 2) uniform sampler2D lowResDepthTexture;
layout(set = 3, binding = 3) uniform sampler2D lowResNormalTexture;

// (localSize / 2 + 2)^2
shared float depths[36];
shared vec3 normals[36];
shared vec3 data[36];

const uint depthDataSize = (gl_WorkGroupSize.x / 2 + 2) * (gl_WorkGroupSize.y / 2 + 2);
const ivec2 unflattenedDepthDataSize = ivec2(gl_WorkGroupSize) / 2 + 2;

void LoadGroupSharedData() {

    ivec2 workGroupOffset = ivec2(gl_WorkGroupID) * ivec2(gl_WorkGroupSize) / 2 - ivec2(1);

    // We assume data size is smaller than gl_WorkGroupSize.x + gl_WorkGroupSize.y
    if (gl_LocalInvocationIndex < depthDataSize) {
        ivec2 offset = Unflatten2D(int(gl_LocalInvocationIndex), unflattenedDepthDataSize);
        offset += workGroupOffset;
        offset = clamp(offset, ivec2(0), textureSize(lowResDepthTexture, 0));
        depths[gl_LocalInvocationIndex] = ConvertDepthToViewSpaceDepth(texelFetch(lowResDepthTexture, offset, 0).r);

        vec3 normal = DecodeNormal(texelFetch(lowResNormalTexture, offset, 0).rg);
        normals[gl_LocalInvocationIndex] = normalize(normal);

        data[gl_LocalInvocationIndex] = texelFetch(lowResTexture, offset, 0).rgb;
    }

    barrier();

}

const ivec2 offsets[9] = ivec2[9](
    ivec2(-1, -1),
    ivec2(0, -1),
    ivec2(1, -1),
    ivec2(-1, 0),
    ivec2(0, 0),
    ivec2(1, 0),
    ivec2(-1, 1),
    ivec2(0, 1),
    ivec2(1, 1)
);

const ivec2 pixelOffsets[4] = ivec2[4](
    ivec2(0, 0),
    ivec2(1, 0),
    ivec2(0, 1),
    ivec2(1, 1)
);

vec4 Upsample(float referenceDepth, vec3 referenceNormal, vec2 highResPixel) {

    vec4 result = vec4(0.0);

    highResPixel /= 2.0;
    float x = fract(highResPixel.x);
    float y = fract(highResPixel.y);

    float weights[4] = { (1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y };

    ivec2 pixel = ivec2(gl_LocalInvocationID) / 2 + ivec2(1);

    referenceDepth = ConvertDepthToViewSpaceDepth(referenceDepth);

    float totalWeight = 0.0;
    float maxWeight = 0.0;
    int closestMemoryOffset = 0;

    for (uint i = 0; i < 4; i++) {
        int sharedMemoryOffset = Flatten2D(pixel + pixelOffsets[i], unflattenedDepthDataSize);
        float depth = depths[sharedMemoryOffset];

        float depthDiff = abs(referenceDepth - depth);
        float depthWeight = min(exp(-depthDiff), 1.0);

        float normalWeight = min(pow(max(dot(referenceNormal, normals[sharedMemoryOffset]), 0.0), 256.0), 1.0);

        float weight = depthWeight * normalWeight * weights[i];
        if (weight > maxWeight) {
            maxWeight = weight;
            closestMemoryOffset = sharedMemoryOffset;
        }

    }

    result = vec4(data[closestMemoryOffset], 1.0);

    return result;

}

void main() {

    LoadGroupSharedData();

    if (gl_GlobalInvocationID.x > imageSize(image).x ||
        gl_GlobalInvocationID.y > imageSize(image).y)
        return;

    ivec2 resolution = imageSize(image);
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(resolution);

    float depth = texelFetch(depthTexture, pixel, 0).r;
    vec3 indirect = vec3(0.0);

    vec3 geometryNormal;
    Surface surface = GetSurface(texCoord, depth, vec3(0.0, -1.0, 0.0), geometryNormal);

    vec4 upsampleResult = Upsample(depth, surface.N, vec2(pixel));

    imageStore(image, pixel, upsampleResult);

}