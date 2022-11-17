#include <../common/utility.hsh>
#include <../common/convert.hsh>
#include <../common/PI.hsh>
#include <../common/stencil.hsh>
#include <../common/flatten.hsh>
#include <../common/material.hsh>
#include <../common/random.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(binding = 0, rgba16f) writeonly uniform image2D resolveImage;
layout(binding = 1, rgba16f) writeonly uniform image2D momentsImage;

layout(binding = 0) uniform sampler2D historyTexture;
layout(binding = 1) uniform sampler2D currentTexture;
layout(binding = 2) uniform sampler2D velocityTexture;
layout(binding = 3) uniform sampler2D depthTexture;
layout(binding = 4) uniform sampler2D roughnessMetallicAoTexture;
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

float Luma(vec3 color) {

    const vec3 luma = vec3(0.299, 0.587, 0.114);
    return dot(color, luma);

}

vec3 FetchTexel(ivec2 texel) {
	
	vec3 color = max(texelFetch(currentTexture, texel, 0).rgb, 0);
	return color;

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

float ClipBoundingBox(vec3 boxMin, vec3 boxMax, vec3 history, vec3 current) {

    vec3 origin = history;
    vec3 dir = current - history;

    // Make sure dir isn't zero
    dir.x = abs(dir.x) < (1.0 / 32767.0) ? (1.0 / 32767.0) : dir.x;
    dir.y = abs(dir.y) < (1.0 / 32767.0) ? (1.0 / 32767.0) : dir.y;
    dir.z = abs(dir.z) < (1.0 / 32767.0) ? (1.0 / 32767.0) : dir.z;

    vec3 invDir = 1.0 / dir;

    vec3 t0 = (boxMin - origin) * invDir;
    vec3 t1 = (boxMax - origin) * invDir;

    vec3 intersect = min(t0, t1);
    return max(intersect.x, max(intersect.y, intersect.z));

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

    vec4 sample0 = texture(historyTexture, uv0) * weight0;
    vec4 sample1 = texture(historyTexture, uv1) * weight1;
    vec4 sample2 = texture(historyTexture, uv2) * weight2;
    vec4 sample3 = texture(historyTexture, uv3) * weight3;
    vec4 sample4 = texture(historyTexture, uv4) * weight4;

    float totalWeight = weight0 + weight1 + 
        weight2 + weight3 + weight4;

    vec4 totalSample = sample0 + sample1 +
        sample2 + sample3 + sample4;

    return totalSample / totalWeight;    

}

vec4 SampleHistory(vec2 texCoord) {

    vec4 historyColor;

    historyColor = SampleCatmullRom(texCoord);

    return historyColor;

}

vec4 SampleHistoryMoments(vec2 texCoord) {

    vec4 moments;

    moments = texture(historyMomentsTexture, texCoord);

    return moments;

}

void ComputeVarianceMinMax(out vec3 aabbMin, out vec3 aabbMax) {

    vec3 m1 = vec3(0.0);
    vec3 m2 = vec3(0.0);
    // This could be varied using the temporal variance estimation
    // By using a wide neighborhood for variance estimation (8x8) we introduce block artifacts
    // These are similiar to video compression artifacts, the spatial filter mostly clears them up
    const int radius = 8;
    ivec2 pixel = ivec2(gl_GlobalInvocationID);

    float depth = texelFetch(depthTexture, pixel, 0).r;
    float linearDepth = ConvertDepthToViewSpaceDepth(depth);

    uint materialIdx = texelFetch(materialIdxTexture, pixel, 0).r;

    vec3 normal = 2.0 * texelFetch(normalTexture, pixel, 0).rgb - 1.0;

    for (int i = -radius; i <= radius; i++) {
        for (int j = -radius; j <= radius; j++) {
            vec3 color = texelFetch(currentTexture, pixel + ivec2(i, j), 0).rgb;

            float historyDepth = texelFetch(depthTexture, pixel + ivec2(i, j), 0).r;
            float historyLinearDepth = ConvertDepthToViewSpaceDepth(historyDepth);
            float weight = min(1.0 , exp(-abs(linearDepth - historyLinearDepth)));
            weight = 1.0;

            uint historyMaterialIdx = texelFetch(materialIdxTexture, pixel + ivec2(i, j), 0).r;
            weight *= historyMaterialIdx != materialIdx ? 0.0 : 1.0;

            vec3 historyNormal = 2.0 * texelFetch(normalTexture, pixel + ivec2(i, j), 0).rgb - 1.0;
            weight *= pow(abs(dot(historyNormal, normal)), 2.0);

            color *= weight;
        
            m1 += color;
            m2 += color * color;
        }
    }

    float oneDividedBySampleCount = 1.0 / float((2.0 * radius + 1.0) * (2.0 * radius + 1.0));
    float gamma = 1.0;
    vec3 mu = m1 * oneDividedBySampleCount;
    vec3 sigma = sqrt(max((m2 * oneDividedBySampleCount) - (mu * mu), 0.0));
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

    vec3 localNeighbourhoodMin, localNeighbourhoodMax;
    ComputeVarianceMinMax(localNeighbourhoodMin, localNeighbourhoodMax);

    ivec2 velocityPixel = pixel;
    vec2 velocity = texelFetch(velocityTexture, velocityPixel, 0).rg;

    vec2 uv = (vec2(pixel) + vec2(0.5)) * invResolution + velocity;

    // Maybe we might want to filter the current input pixel
    vec4 history = SampleHistory(uv);
    vec4 historyMoments = SampleHistoryMoments(uv);

    vec3 historyColor = history.rgb;
    vec3 currentColor = texelFetch(currentTexture, pixel, 0).rgb;

    vec2 currentMoments;
    currentMoments.r = Luma(currentColor);
    currentMoments.g = currentMoments.r * currentMoments.r;

    // In case of clipping we might also reject the sample. TODO: Investigate
    float clipBlend = ClipBoundingBox(localNeighbourhoodMin, localNeighbourhoodMax,
        historyColor, currentColor);
    float adjClipBlend = saturate(clipBlend);
    historyColor = mix(historyColor, currentColor, adjClipBlend);

    uint materialIdx = texelFetch(materialIdxTexture, pixel, 0).r;
	Material material = UnpackMaterial(materialIdx);

    float roughness = texelFetch(roughnessMetallicAoTexture, pixel, 0).r;
    material.roughness *= material.roughnessMap ? roughness : 1.0;

    float factor = clamp(16.0 * log(material.roughness + 1.0), 0.001, 0.95);
    factor = (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0
         || uv.y > 1.0) ? 0.0 : factor;

    vec3 normal = 2.0 * texelFetch(normalTexture, pixel, 0).rgb - 1.0;
    float depth = texelFetch(depthTexture, pixel, 0).r;

    float linearDepth = ConvertDepthToViewSpaceDepth(depth);

    ivec2 historyPixel = ivec2(vec2(pixel) + vec2(0.5) + velocity * resolution);
    float maxConfidence = 0.0;
    // Calculate confidence over 2x2 bilinear neighborhood
    // Note that 3x3 neighborhoud could help on edges
    for (int i = 0; i < 9; i++) {
        ivec2 offsetPixel = historyPixel + offsets[i];
        float confidence = 1.0;

        uint historyMaterialIdx = texelFetch(materialIdxTexture, offsetPixel, 0).r;
        //confidence *= historyMaterialIdx != materialIdx ? 0.1 : 1.0;

        vec3 historyNormal = 2.0 * texelFetch(normalTexture, offsetPixel, 0).rgb - 1.0;
        //confidence *= pow(abs(dot(historyNormal, normal)), 2.0);

        float historyDepth = texelFetch(depthTexture, offsetPixel, 0).r;
        float historyLinearDepth = ConvertDepthToViewSpaceDepth(historyDepth);
        //confidence *= min(1.0 , exp(-abs(linearDepth - historyLinearDepth)));

        confidence = exp(-abs(1.0 - max(0.0, dot(normal, historyNormal))))
            * exp(-abs(historyLinearDepth - linearDepth));

        maxConfidence = max(maxConfidence, confidence);
    }
    
    factor *= maxConfidence;

    float historyLength = historyMoments.b;
    if (factor <= 0.1) {
        historyLength = 0.0;
        currentMoments.g = 1.0;
        currentMoments.r = 0.0;
    }

    factor = min(factor, historyLength / (historyLength + 1.0));

    vec3 resolve = mix(currentColor, historyColor, factor);
    vec2 momentsResolve = mix(currentMoments, historyMoments.rg, factor);

    // Boost variance when we have a small history length (we trade blur for noise)
    float varianceBoost = max(1.0, 4.0 / (historyLength + 1.0));
    float variance = max(0.0, momentsResolve.g - momentsResolve.r * momentsResolve.r);
    variance *= varianceBoost;

    // No spatial filtering for mirror reflections
    variance = material.roughness < 0.001 ? 0.0 : variance;

    imageStore(momentsImage, pixel, vec4(momentsResolve, historyLength + 1.0, 0.0));
    imageStore(resolveImage, pixel, vec4(vec3(resolve), variance));

}