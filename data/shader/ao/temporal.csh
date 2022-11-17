#include <../common/utility.hsh>
#include <../common/convert.hsh>
#include <../common/PI.hsh>
#include <../common/stencil.hsh>
#include <../common/flatten.hsh>
#include <../common/random.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(binding = 0, r16f) writeonly uniform image2D resolveImage;
layout(binding = 1, r16f) writeonly uniform image2D momentsImage;

layout(binding = 0) uniform sampler2D historyTexture;
layout(binding = 1) uniform sampler2D currentTexture;
layout(binding = 2) uniform sampler2D velocityTexture;
layout(binding = 3) uniform sampler2D depthTexture;
layout(binding = 6) uniform sampler2D normalTexture;
layout(binding = 7) uniform usampler2D materialIdxTexture;
layout(binding = 10) uniform sampler2D historyMomentsTexture;

uniform vec2 invResolution;
uniform vec2 resolution;

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

float FetchTexel(ivec2 texel) {
	
	float value = max(texelFetch(currentTexture, texel, 0).r, 0);
	return value;

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

float SampleCatmullRom(vec2 uv) {

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

    float sample0 = texture(historyTexture, uv0).r * weight0;
    float sample1 = texture(historyTexture, uv1).r * weight1;
    float sample2 = texture(historyTexture, uv2).r * weight2;
    float sample3 = texture(historyTexture, uv3).r * weight3;
    float sample4 = texture(historyTexture, uv4).r * weight4;

    float totalWeight = weight0 + weight1 + 
        weight2 + weight3 + weight4;

    float totalSample = sample0 + sample1 +
        sample2 + sample3 + sample4;

    return totalSample / totalWeight;    

}

float SampleHistory(vec2 texCoord) {

    float historyValue;

    historyValue = SampleCatmullRom(texCoord);

    return historyValue;

}

vec4 SampleHistoryMoments(vec2 texCoord) {

    vec4 moments;

    moments = texture(historyMomentsTexture, texCoord);

    return moments;

}

void ComputeVarianceMinMax(out float aabbMin, out float aabbMax) {

    float m1 = 0.0;
    float m2 = 0.0;
    // This could be varied using the temporal variance estimation
    // By using a wide neighborhood for variance estimation (8x8) we introduce block artifacts
    // These are similiar to video compression artifacts, the spatial filter mostly clears them up
    const int radius = 4;
    ivec2 pixel = ivec2(gl_GlobalInvocationID);

    uint materialIdx = texelFetch(materialIdxTexture, pixel, 0).r;

    float depth = texelFetch(depthTexture, pixel, 0).r;
    float linearDepth = ConvertDepthToViewSpaceDepth(depth);

    for (int i = -radius; i <= radius; i++) {
        for (int j = -radius; j <= radius; j++) {
            float value = texelFetch(currentTexture, pixel + ivec2(i, j), 0).r;

            uint historyMaterialIdx = texelFetch(materialIdxTexture, pixel + ivec2(i, j), 0).r;
            float materialWeight = historyMaterialIdx != materialIdx ? 0.0 : 1.0;

            float historyDepth = texelFetch(depthTexture, pixel + ivec2(i, j), 0).r;
            float historyLinearDepth = ConvertDepthToViewSpaceDepth(historyDepth);
            float depthWeight = min(1.0 , exp(-abs(linearDepth - historyLinearDepth)));

            value *= depthWeight * materialWeight;
        
            m1 += value;
            m2 += value * value;
        }
    }

    float oneDividedBySampleCount = 1.0 / float((2.0 * radius + 1.0) * (2.0 * radius + 1.0));
    float gamma = 1.0;
    float mu = m1 * oneDividedBySampleCount;
    float sigma = sqrt(max((m2 * oneDividedBySampleCount) - (mu * mu), 0.0));
    aabbMin = mu - gamma * sigma;
    aabbMax = mu + gamma * sigma;

}

float ComputeDisocclusionWeight(vec3 normal, vec3 historyNormal,
                                float historyLinearDepth, float linearDepth) {
    return exp(-abs(1.0 - max(0.0, dot(normal, historyNormal))))
        * exp(-abs(historyLinearDepth - linearDepth) / linearDepth);
}

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(resolveImage).x ||
        pixel.y > imageSize(resolveImage).y)
        return;

    float localNeighbourhoodMin, localNeighbourhoodMax;
    ComputeVarianceMinMax(localNeighbourhoodMin, localNeighbourhoodMax);

    ivec2 velocityPixel = pixel;
    vec2 velocity = texelFetch(velocityTexture, velocityPixel, 0).rg;

    vec2 uv = (vec2(pixel) + vec2(0.5)) * invResolution + velocity;

    // Maybe we might want to filter the current input pixel
    float history = SampleHistory(uv);
    vec4 historyMoments = SampleHistoryMoments(uv);

    float historyValue = history;
    float currentValue = texelFetch(currentTexture, pixel, 0).r;

    vec2 currentMoments;
    //currentMoments.r = Luma(currentColor);
    //currentMoments.g = currentMoments.r * currentMoments.r;

    // In case of clipping we might also reject the sample. TODO: Investigate
    historyValue = clamp(historyValue, localNeighbourhoodMin, localNeighbourhoodMax);

    uint materialIdx = texelFetch(materialIdxTexture, pixel, 0).r;

    float factor = 0.9;
    factor = (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0
         || uv.y > 1.0) ? 0.0 : factor;

    vec3 normal = 2.0 * texelFetch(normalTexture, pixel, 0).rgb - 1.0;
    float depth = texelFetch(depthTexture, pixel, 0).r;

    float linearDepth = ConvertDepthToViewSpaceDepth(depth);

    ivec2 historyPixel = ivec2(vec2(pixel) + velocity * resolution);
    float maxConfidence = 0.0;
    // Calculate confidence over 2x2 bilinear neighborhood
    // Note that 3x3 neighborhoud could help on edges
    for (int i = 0; i < 9; i++) {
        ivec2 offsetPixel = historyPixel + offsets[i];
        float confidence = 1.0;

        uint historyMaterialIdx = texelFetch(materialIdxTexture, offsetPixel, 0).r;
        confidence *= historyMaterialIdx != materialIdx ? 0.0 : 1.0;

        vec3 historyNormal = 2.0 * texelFetch(normalTexture, offsetPixel, 0).rgb - 1.0;
        //confidence *= pow(abs(dot(historyNormal, normal)), 2.0);

        float historyDepth = texelFetch(depthTexture, offsetPixel, 0).r;
        float historyLinearDepth = ConvertDepthToViewSpaceDepth(historyDepth);
        //confidence *= min(1.0 , exp(-abs(linearDepth - historyLinearDepth) / linearDepth));

        maxConfidence = max(maxConfidence, confidence);
    }
    
    factor *= maxConfidence;

    float historyLength = historyMoments.b;
    if (factor == 0.0) {
        historyLength = 0.0;
        currentMoments.g = 1.0;
        currentMoments.r = 0.0;
    }

    factor = min(factor, historyLength / (historyLength + 1.0));
    factor = max(0.5, factor);

    float resolve = mix(currentValue, historyValue, factor);
    vec2 momentsResolve = mix(currentMoments, historyMoments.rg, factor);

    // Boost variance when we have a small history length (we trade blur for noise)
    /*
    float varianceBoost = max(1.0, 4.0 / (historyLength + 1.0));
    float variance = max(0.0, momentsResolve.g - momentsResolve.r * momentsResolve.r);
    variance *= varianceBoost;
    */

    imageStore(momentsImage, pixel, vec4(momentsResolve, historyLength + 1.0, 0.0));
    imageStore(resolveImage, pixel, vec4(resolve, 0.0, 0.0, 0.0));

}