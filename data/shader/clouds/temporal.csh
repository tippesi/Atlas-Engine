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

vec4 SampleCatmullRom(vec2 uv) {

    // http://advances.realtimerendering.com/s2016/Filmic%20SMAA%20v7.pptx
    // Credit: Jorge Jimenez (SIGGRAPH 2016)
    // Ignores the 4 corners of the 4x4 grid
    // Learn more: http://vec3.ca/bicubic-filtering-in-fewer-taps/
    vec2 position = uv * resolution;

    vec2 center = floor(position - 0.5) + 0.5;
    vec2 f = position - center;
    vec2 f2 = f * f;
    vec2 f3 = f2 * f;

    vec2 w0 = f2 - 0.5 * (f3 + f);
    vec2 w1 = 1.5 * f3 - 2.5 * f2 + 1.0;
    vec2 w3 = 0.5 * (f3 - f2);
    vec2 w2 = 1.0 - w0 - w1 - w3;

    vec2 w12 = w1 + w2;

    vec2 tc0 = (center - 1.0) * invResolution;
    vec2 tc12 = (center + w2 / w12) * invResolution;
    vec2 tc3 = (center + 2.0) * invResolution;

    vec2 uv0 = clamp(vec2(tc12.x, tc0.y), vec2(0.0), vec2(1.0));
    vec2 uv1 = clamp(vec2(tc0.x, tc12.y), vec2(0.0), vec2(1.0));
    vec2 uv2 = clamp(vec2(tc12.x, tc12.y), vec2(0.0), vec2(1.0));
    vec2 uv3 = clamp(vec2(tc3.x, tc12.y), vec2(0.0), vec2(1.0));
    vec2 uv4 = clamp(vec2(tc12.x, tc3.y), vec2(0.0), vec2(1.0));

    float weight0 = w12.x * w0.y;
    float weight1 = w0.x * w12.y;
    float weight2 = w12.x * w12.y;
    float weight3 = w3.x * w12.y;
    float weight4 = w12.x * w3.y;

    vec4 sample0 = textureLod(historyTexture, uv0, 0.0) * weight0;
    vec4 sample1 = textureLod(historyTexture, uv1, 0.0) * weight1;
    vec4 sample2 = textureLod(historyTexture, uv2, 0.0) * weight2;
    vec4 sample3 = textureLod(historyTexture, uv3, 0.0) * weight3;
    vec4 sample4 = textureLod(historyTexture, uv4, 0.0) * weight4;

    float totalWeight = weight0 + weight1 + 
        weight2 + weight3 + weight4;

    vec4 totalSample = sample0 + sample1 +
        sample2 + sample3 + sample4;

    return totalSample / totalWeight;    

}

vec4 SampleHistory(vec2 texCoord) {

    vec4 historyValue;

    historyValue = SampleCatmullRom(texCoord);

    return historyValue;

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

    // Maybe we might want to filter the current input pixel
    vec4 history = SampleHistory(uv);

    vec4 historyValue = history;
    vec4 currentValue = texelFetch(currentTexture, pixel, 0);

    historyValue = clamp(historyValue, localNeighbourhoodMin, localNeighbourhoodMax);

    // We don't want to do anything fancy here, just a bit of constant accumulation
    float factor = 0.875;
    factor = (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0
         || uv.y > 1.0) ? 0.0 : factor;

    ivec2 historyPixel = ivec2(vec2(pixel) + velocity * resolution);
    float minConfidence = 1.0;
    // Calculate confidence over 2x2 bilinear neighborhood
    // Note that 3x3 neighborhoud could help on edges
    for (int i = 0; i < 9; i++) {
        ivec2 offsetPixel = historyPixel + offsets[i];
        float confidence = 1.0;

        float historyDepth = texelFetch(historyDepthTexture, offsetPixel, 0).r;
        confidence *= historyDepth < 1.0 ? 0.0 : 1.0;

        minConfidence = min(minConfidence, confidence);
    }
    
    factor *= minConfidence;

    vec4 resolve = mix(currentValue, historyValue, factor);

    imageStore(resolveImage, pixel, resolve);

}