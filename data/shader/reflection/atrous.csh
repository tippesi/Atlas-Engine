#include <../common/utility.hsh>
#include <../common/convert.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(binding = 0) uniform sampler2D inputTexture;
layout(binding = 1) uniform sampler2D depthTexture;
layout(binding = 2) uniform sampler2D normalTexture;
layout(binding = 3) uniform sampler2D roughnessTexture;

layout(binding = 0) writeonly uniform image2D outputImage;

uniform int stepSize;

const float kernelWeights[3] = { 1.0, 2.0 / 3.0, 1.0 / 6.0 };

float Luma(vec3 color) {

    const vec3 luma = vec3(0.299, 0.587, 0.114);
    return dot(color, luma);

}

float ComputeEdgeStoppingWeight(float centerLuminance, float sampleLuminance,
                                vec3 centerNormal, vec3 sampleNormal,
                                float centerDepth, float sampleDepth,
                                float centerRoughness, float sampleRoughness,
                                float luminancePhi, float normalPhi, 
                                float depthPhi, float roughnessPhi) {

    float luminanceDiff = abs(centerLuminance - sampleLuminance);
    float luminanceWeight = min(exp(-luminanceDiff / luminancePhi), 1.0);

    float normalDiff = saturate(dot(centerNormal, sampleNormal));
    float normalWeight = min(pow(normalDiff, normalPhi), 1.0);

    float depthDiff = abs(centerDepth - sampleDepth);
    float depthWeight = min(exp(-depthDiff / depthPhi), 1.0);

    float roughnessDiff = abs(centerRoughness - sampleRoughness);
    float roughnessWeight = min(exp(-roughnessDiff / roughnessPhi), 1.0);

    return luminanceWeight * normalWeight * depthWeight * roughnessWeight;

}

float GetFilteredVariance(ivec2 pixel) {

    float variance = 0.0;
    const float weights[2][2] =
    {
        { 1.0 / 4.0, 1.0 / 8.0  },
        { 1.0 / 8.0, 1.0 / 16.0 }
    };

    const int radius = 1;
    for (int x = -radius; x <= radius; x++) {
        for (int y = -radius; y <= radius; y++) {
            ivec2 samplePixel = pixel + ivec2(x, y);

            float weight = weights[abs(x)][abs(y)];
            float sampleVariance = texelFetch(inputTexture, samplePixel, 0).a;
            variance += weight * sampleVariance;
        }
    }

    return variance;

}

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    ivec2 resolution = imageSize(outputImage);

    if (pixel.x >= resolution.x || pixel.y >= resolution.y)
        return;
    
    vec4 centerColor = texelFetch(inputTexture, pixel, 0).rgba;
    float centerDepth = texelFetch(depthTexture, pixel, 0).r;
    vec3 centerNormal = 2.0 * texelFetch(normalTexture, pixel, 0).rgb - 1.0;

    float centerLuminance = Luma(centerColor.rgb);
    float centerLinearDepth = ConvertDepthToViewSpaceDepth(centerDepth);

    float centerRoughness = texelFetch(roughnessTexture, pixel, 0).r;

    vec4 outputColor = centerColor;
    float totalWeight = 1.0;

    float variance = GetFilteredVariance(pixel);
    float stdDeviation = sqrt(max(0.0, variance));

    const int radius = 2;
    for (int x = -radius; x <= radius; x++) {
        for (int y = -radius; y <= radius; y++) {            
            ivec2 samplePixel = pixel + ivec2(x, y) * stepSize;

            if (samplePixel.x >= resolution.x || samplePixel.y >= resolution.y
                || (x == 0 && y == 0))
                continue;

            float sampleDepth = texelFetch(depthTexture, samplePixel, 0).r;

            if (sampleDepth == 1.0)
                continue;

            vec4 sampleColor = texelFetch(inputTexture, samplePixel, 0);
            vec3 sampleNormal = 2.0 * texelFetch(normalTexture, samplePixel, 0).rgb - 1.0;

            float sampleLinearDepth = ConvertDepthToViewSpaceDepth(sampleDepth);
            float sampleLuminance = Luma(sampleColor.rgb);

            float sampleRoughness = texelFetch(roughnessTexture, samplePixel, 0).r;

            float kernelWeight = kernelWeights[abs(x)] * kernelWeights[abs(y)];
            float edgeStoppingWeight = ComputeEdgeStoppingWeight(
                                    centerLuminance, sampleLuminance,
                                    centerNormal, sampleNormal,
                                    centerLinearDepth, sampleLinearDepth,
                                    centerRoughness, sampleRoughness,
                                    stdDeviation * 5.0, 2.0, 
                                    1.0, 0.05);

            float weight = kernelWeight * edgeStoppingWeight;
            
            totalWeight += weight;
            outputColor += vec4(vec3(weight), weight * weight) * sampleColor;
        }
    }

    outputColor = outputColor / vec4(vec3(totalWeight), totalWeight * totalWeight);

    imageStore(outputImage, pixel, outputColor);

}