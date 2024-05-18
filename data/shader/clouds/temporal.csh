#include <../common/utility.hsh>
#include <../common/convert.hsh>
#include <../common/PI.hsh>
#include <../common/stencil.hsh>
#include <../common/flatten.hsh>
#include <../common/random.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(set = 3, binding = 0, rgba16f) writeonly uniform image2D resolveImage;

layout(set = 3, binding = 1) uniform sampler2D currentTexture;
layout(set = 3, binding = 2) uniform sampler2D velocityTexture;
layout(set = 3, binding = 3) uniform sampler2D depthTexture;

layout(set = 3, binding = 4) uniform sampler2D historyTexture;
layout(set = 3, binding = 5) uniform sampler2D historyDepthTexture;

layout(push_constant) uniform constants {
    int resetHistory;
    int downsampled2x;
} pushConstants;

vec2 invResolution = 1.0 / vec2(imageSize(resolveImage));
vec2 resolution = vec2(imageSize(resolveImage));

const int kernelRadius = 3;

const uint sharedDataSize = (gl_WorkGroupSize.x + 2 * kernelRadius) * (gl_WorkGroupSize.y + 2 * kernelRadius);
const ivec2 unflattenedSharedDataSize = ivec2(gl_WorkGroupSize) + 2 * kernelRadius;

shared vec4 sharedData[sharedDataSize];

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

vec4 FetchTexel(ivec2 texel) {
    
    vec4 value = max(texelFetch(currentTexture, texel, 0), 0);
    return value;

}

void LoadGroupSharedData() {

    ivec2 workGroupOffset = ivec2(gl_WorkGroupID) * ivec2(gl_WorkGroupSize) - ivec2(kernelRadius);

    uint workGroupSize = gl_WorkGroupSize.x * gl_WorkGroupSize.y;
    for(uint i = gl_LocalInvocationIndex; i < sharedDataSize; i += workGroupSize) {
        ivec2 localOffset = Unflatten2D(int(i), unflattenedSharedDataSize);
        ivec2 texel = localOffset + workGroupOffset;

        texel = clamp(texel, ivec2(0), ivec2(resolution) - ivec2(1));

        sharedData[i] = FetchTexel(texel);
    }

    barrier();

}

int GetSharedMemoryIndex(ivec2 pixelOffset) {

    ivec2 pixel = ivec2(gl_LocalInvocationID) + ivec2(kernelRadius);
    return Flatten2D(pixel + pixelOffset, unflattenedSharedDataSize);

}

vec4 FetchCurrentColor(int sharedMemoryIdx) {

    return sharedData[sharedMemoryIdx];

}

ivec2 FindNearest3x3(ivec2 pixel) {

    ivec2 offset = ivec2(0);
    float depth = 1.0;

    for (int i = 0; i < 9; i++) {
        float currDepth = texelFetch(depthTexture, pixel + offsets[i], 0).r;
        if (currDepth < depth) {
            depth = currDepth;
            offset = offsets[i];
        }
    }

    return offset;

}

bool SampleHistory(ivec2 pixel, vec2 historyPixel, out vec4 history) {
    
    history = vec4(0.0);

    float totalWeight = 0.0;
    float x    = fract(historyPixel.x);
    float y    = fract(historyPixel.y);

    float weights[4] = { (1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y };

    float depth = texelFetch(depthTexture, pixel, 0).r;
    float linearDepth = ConvertDepthToViewSpaceDepth(depth);

    // Calculate confidence over 2x2 bilinear neighborhood
    for (int i = 0; i < 4; i++) {
        ivec2 offsetPixel = ivec2(historyPixel) + pixelOffsets[i];
        float confidence = 1.0;

        offsetPixel = clamp(offsetPixel, ivec2(0), ivec2(resolution) - ivec2(1));

        float historyDepth = texelFetch(historyDepthTexture, offsetPixel, 0).r;
        float historyLinearDepth = ConvertDepthToViewSpaceDepth(historyDepth);
        confidence *= min(1.0 , exp(-abs(linearDepth - historyLinearDepth)));

        if (confidence > 0.5) {
            totalWeight += weights[i];
            history += texelFetch(historyTexture, offsetPixel, 0) * weights[i];
        }
    }

    if (totalWeight > 0.0) {
        history /= totalWeight;
        return true;
    }

    for (int i = 0; i < 9; i++) {
        ivec2 offsetPixel = ivec2(historyPixel) + offsets[i];
        float confidence = 1.0;

        offsetPixel = clamp(offsetPixel, ivec2(0), ivec2(resolution) - ivec2(1));

        float historyDepth = texelFetch(historyDepthTexture, offsetPixel, 0).r;
        float historyLinearDepth = ConvertDepthToViewSpaceDepth(historyDepth);
        confidence *= min(1.0 , exp(-abs(linearDepth - historyLinearDepth)));

        if (confidence > 0.5) {
            totalWeight += 1.0;
            history += texelFetch(historyTexture, offsetPixel, 0);
        }
    }

    if (totalWeight > 0.0) {
        history /= totalWeight;
        return true;
    }

    history = vec4(0.0);

    return false;

}

float IsHistoryPixelValid(ivec2 pixel, float linearDepth) {

    float depthPhi = max(1.0, abs(0.25 * linearDepth));
    float historyDepth = texelFetch(historyDepthTexture, pixel, 0).r;
    float historyLinearDepth = historyDepth;
    float confidence = min(1.0 , exp(-abs(linearDepth - historyLinearDepth)));

    return confidence > 0.1 ? 1.0 : 0.0;

}

vec4 GetCatmullRomSample(ivec2 pixel, inout float weight, float linearDepth) {

    pixel = clamp(pixel, ivec2(0), ivec2(imageSize(resolveImage) - 1));

    weight *= IsHistoryPixelValid(pixel, linearDepth);

    return texelFetch(historyTexture, pixel, 0) * weight;

}

bool SampleCatmullRom(ivec2 pixel, vec2 uv, out vec4 history) {
    
    float depth = texelFetch(depthTexture, pixel, 0).r;

    vec2 position = uv * resolution;

    vec2 center = floor(position - 0.5) + 0.5;
    vec2 f = position - center;
    vec2 f2 = f * f;
    vec2 f3 = f2 * f;

    vec2 w0 = f2 - 0.5 * (f3 + f);
    vec2 w1 = 1.5 * f3 - 2.5 * f2 + 1.0;
    vec2 w3 = 0.5 * (f3 - f2);
    vec2 w2 = 1.0 - w0 - w1 - w3;

    ivec2 uv0 = ivec2(center - 1.0);
    ivec2 uv1 = ivec2(center);
    ivec2 uv2 = ivec2(center + 1.0);
    ivec2 uv3 = ivec2(center + 2.0);

    ivec2 uvs[4] = { uv0, uv1, uv2, uv3 };
    vec2 weights[4] = { w0, w1, w2, w3 };

    history = vec4(0.0);

    float totalWeight = 0.0;

    for (int x = 0; x <= 3; x++) {
        for (int y = 0; y <= 3; y++) {
            float weight = weights[x].x * weights[y].y;
            ivec2 uv = ivec2(uvs[x].x, uvs[y].y);

            history += GetCatmullRomSample(uv, weight, depth);
            totalWeight += weight;
        }
    }
    
    if (totalWeight > 0.5) {
        history /= totalWeight;
   
        return true;
    }

    return false;
}

void ComputeVarianceMinMax(out vec4 aabbMin, out vec4 aabbMax) {

    vec4 m1 = vec4(0.0);
    vec4 m2 = vec4(0.0);

    const int radius = 3;
    ivec2 pixel = ivec2(gl_GlobalInvocationID);

    for (int i = -radius; i <= radius; i++) {
        for (int j = -radius; j <= radius; j++) {
            int sharedMemoryIdx = GetSharedMemoryIndex(ivec2(i, j));

            vec4 sampleColor = FetchCurrentColor(sharedMemoryIdx);
        
            m1 += sampleColor;
            m2 += sampleColor * sampleColor;
        }
    }

    float oneDividedBySampleCount = 1.0 / float((2.0 * radius + 1.0) * (2.0 * radius + 1.0));
    float gamma = 1.0;
    vec4 mu = m1 * oneDividedBySampleCount;
    vec4 sigma = sqrt(max((m2 * oneDividedBySampleCount) - (mu * mu), 0.0));
    aabbMin = mu - gamma * sigma;
    aabbMax = mu + gamma * sigma;

}

void main() {

    // We could take care of disocclusions by looking if we are out of the cloud layer.
    // In between it is really hard to check for disocclusions
    LoadGroupSharedData();

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(resolveImage).x ||
        pixel.y > imageSize(resolveImage).y)
        return;

    vec4 localNeighbourhoodMin, localNeighbourhoodMax;
    ComputeVarianceMinMax(localNeighbourhoodMin, localNeighbourhoodMax);

    ivec2 velocityPixel = pixel;
    vec2 velocity = texelFetch(velocityTexture, velocityPixel, 0).rg;

    vec2 uv = (vec2(pixel) + vec2(0.5)) * invResolution + velocity;
    vec2 historyPixel = vec2(pixel) + velocity * resolution;

    // Maybe we might want to filter the current input pixel
    vec4 history = vec4(0.0);

    bool valid = SampleHistory(pixel, historyPixel, history);

    // We only really need this for low resolution inputs
    if (pushConstants.downsampled2x > 0) {
        vec4 catmullRomHistory;
        bool success = SampleCatmullRom(pixel, uv, catmullRomHistory);
        history = success && valid ? catmullRomHistory : history;  
    }

    vec4 historyValue = history;
    vec4 currentValue = texelFetch(currentTexture, pixel, 0);

    float currentDepth = texelFetch(depthTexture, pixel, 0).r;

    historyValue = clamp(historyValue, localNeighbourhoodMin, localNeighbourhoodMax);

    // We don't want to do anything fancy here, just a bit of constant accumulation
    float factor = 0.875;
    factor = (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0
         || uv.y > 1.0) ? 0.0 : factor;

    factor *= valid ? 1.0 : 0.0;
    factor = pushConstants.resetHistory > 0 ? 0.0 : factor;

    vec4 resolve = factor <= 0.0 ? currentValue : mix(currentValue, historyValue, factor);

    imageStore(resolveImage, pixel, resolve);

}