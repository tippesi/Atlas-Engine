#include <common/utility.hsh>
#include <common/PI.hsh>
#include <common/stencil.hsh>
#include <common/flatten.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(set = 3, binding = 0, rgba16f) writeonly uniform image2D resolveImage;

layout(set = 3, binding = 1) uniform sampler2D historyTexture;
layout(set = 3, binding = 2) uniform sampler2D currentTexture;
layout(set = 3, binding = 3) uniform sampler2D velocityTexture;
layout(set = 3, binding = 4) uniform sampler2D depthTexture;
layout(set = 3, binding = 5) uniform sampler2D lastVelocityTexture;

#ifndef PATHTRACE
layout(set = 3, binding = 6) uniform usampler2D stencilTexture;
#endif

layout(push_constant) uniform constants {
    vec2 resolution;
    vec2 invResolution;
    vec2 jitter;
} PushConstants;

const float minVelocityBlend = 0.05;
const float maxVelocityBlend = 0.5;

#define TAA_YCOCG
#define TAA_BICUBIC // Nearly always use the bicubic sampling for better quality and sharpness under movement
#define TAA_TONE // Somehow introduces more flickering as well
#define TAA_CLIP
//#define TAA_DENOISE

#ifdef PATHTRACE
#define TAA_CLIP
#endif

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

// (localSize + 2)^2
shared vec4 sharedNeighbourhood[100];

vec3 localNeighbourhood[9];
float localDepths[9];

const mat3 RGBToYCoCgMatrix = mat3(0.25, 0.5, -0.25, 0.5, 0.0, 0.5, 0.25, -0.5, -0.25);
const mat3 YCoCgToRGBMatrix = mat3(1.0, 1.0, 1.0, 1.0, 0.0, -1.0, -1.0, 1.0, -1.0);

const uint sharedDataSize = (gl_WorkGroupSize.x + 2) * (gl_WorkGroupSize.y + 2);
const ivec2 unflattenedSharedDataSize = ivec2(gl_WorkGroupSize) + 2;

vec3 RGBToYCoCg(vec3 RGB) {

    return RGBToYCoCgMatrix * RGB;

}

vec3 YCoCgToRGB(vec3 YCoCg) {

    return YCoCgToRGBMatrix * YCoCg;

}

float Luma(vec3 color) {

#ifdef TAA_YCOCG
    return color.r;
#else
    const vec3 luma = vec3(0.299, 0.587, 0.114);
    return dot(color, luma);
#endif

}

vec3 Tonemap(vec3 color) {
    
    return color / (1.0 + Luma(color));
    
}

vec3 InverseTonemap(vec3 color) {
    
    return color / (1.0 - Luma(color));
    
}

vec3 FetchTexel(ivec2 texel) {
    
    vec3 color = max(texelFetch(currentTexture, texel, 0).rgb, 0);

#ifdef TAA_TONE
    color = Tonemap(color);
#endif

#ifdef TAA_YCOCG
    color = RGBToYCoCg(color);
#endif

    return color;

}

void LoadGroupSharedData() {

    ivec2 workGroupOffset = ivec2(gl_WorkGroupID) * ivec2(gl_WorkGroupSize) - ivec2(1);

    uint workGroupSize = gl_WorkGroupSize.x * gl_WorkGroupSize.y;
    for(uint i = gl_LocalInvocationIndex; i < sharedDataSize; i += workGroupSize) {
        ivec2 localOffset = Unflatten2D(int(i), unflattenedSharedDataSize);
        ivec2 texel = localOffset + workGroupOffset;

        texel = clamp(texel, ivec2(0), ivec2(PushConstants.resolution) - ivec2(1));

        float depth = texelFetch(depthTexture, texel, 0).r;
        sharedNeighbourhood[i] = vec4(FetchTexel(texel), depth);
    }

    barrier();

} 

void LoadLocalData() {

    ivec2 pixel = ivec2(gl_LocalInvocationID) + ivec2(1);

    for (uint i = 0; i < 9; i++) {
        int sharedMemoryOffset = Flatten2D(pixel + offsets[i], unflattenedSharedDataSize);

        vec4 data = sharedNeighbourhood[sharedMemoryOffset];
        localNeighbourhood[i] = data.rgb;
        localDepths[i] = data.a;
    }

}

ivec2 FindNearest3x3() {

    ivec2 offset = ivec2(0);
    float depth = 1.0;

    for (int i = 0; i < 9; i++) {
        float currDepth = localDepths[i];
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
    vec2 position = uv * PushConstants.resolution;

    vec2 center = floor(position - 0.5) + 0.5;
    vec2 f = position - center;
    vec2 f2 = f * f;
    vec2 f3 = f2 * f;

    vec2 w0 = f2 - 0.5 * (f3 + f);
    vec2 w1 = 1.5 * f3 - 2.5 * f2 + 1.0;
    vec2 w3 = 0.5 * (f3 - f2);
    vec2 w2 = 1.0 - w0 - w1 - w3;

    vec2 w12 = w1 + w2;

    vec2 tc0 = (center - 1.0) * PushConstants.invResolution;
    vec2 tc12 = (center + w2 / w12) * PushConstants.invResolution;
    vec2 tc3 = (center + 2.0) * PushConstants.invResolution;

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

#ifdef TAA_BICUBIC
    historyColor = SampleCatmullRom(texCoord);
#else
    historyColor = texture(historyTexture, texCoord);
#endif

#ifdef TAA_TONE
    historyColor.rgb = Tonemap(historyColor.rgb);
#endif

#ifdef TAA_YCOCG
    historyColor.rgb = RGBToYCoCg(historyColor.rgb);
#endif

    return historyColor;

}

float FilterBlackmanHarris(float x) {

    x = 1.0 - abs(x);

    const float a0 = 0.35875;
    const float a1 = 0.48829;
    const float a2 = 0.14128;
    const float a3 = 0.01168;
    return saturate(a0 - a1 * cos(PI * x) + a2 * cos(2 * PI * x) - a3 * cos(3 * PI * x));

}

vec3 SampleCurrent() {

    vec3 filtered = vec3(0.0);

    float sumWeights = 0.0;
    for (int i = 0; i < 9; i++) {
        vec3 color = InverseTonemap(YCoCgToRGB(localNeighbourhood[i]));
        vec2 offset = vec2(offsets[i]);

        //float weight = exp(-2.29 * (offset.x * offset.x + offset.y * offset.y));
        float weight = FilterBlackmanHarris(offset.x) * FilterBlackmanHarris(offset.y);

        filtered += color * weight;
        sumWeights += weight;
    }

    return RGBToYCoCg(Tonemap(max(filtered / sumWeights, 0.0)));

}

void ComputeVarianceMinMax(out vec3 aabbMin, out vec3 aabbMax) {

    vec3 m1 = vec3(0.0);
    vec3 m2 = vec3(0.0);
    for (int i = 0; i < 9; i++) {
        vec3 color = localNeighbourhood[i];
        
        m1 += color;
        m2 += color * color;
    }

    float oneDividedBySampleCount = 1.0 / 9.0;
    float gamma = 1.0;
    vec3 mu = m1 * oneDividedBySampleCount;
    vec3 sigma = sqrt(abs((m2 * oneDividedBySampleCount) - (mu * mu)));
    aabbMin = mu - gamma * sigma;
    aabbMax = mu + gamma * sigma;

}

void main() {

    LoadGroupSharedData();

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(resolveImage).x ||
        pixel.y > imageSize(resolveImage).y)
        return;

    LoadLocalData();

    // Find nearest pixel in localNeighbourhood to improve velocity sampling
    ivec2 offset = FindNearest3x3();

    vec3 tl = localNeighbourhood[0];
    vec3 tc = localNeighbourhood[1];
    vec3 tr = localNeighbourhood[2];
    vec3 ml = localNeighbourhood[3];
    vec3 mc = localNeighbourhood[4];
    vec3 mr = localNeighbourhood[5];
    vec3 bl = localNeighbourhood[6];
    vec3 bc = localNeighbourhood[7];
    vec3 br = localNeighbourhood[8];

    // 3x3 box pattern
    vec3 boxMin = min(tl, min(tc, min(tr, min(ml, min(mc, min(mr, min(bl, min(bc, br))))))));
    vec3 boxMax = max(tl, max(tc, max(tr, max(ml, max(mc, max(mr, max(bl, max(bc, br))))))));

    // 5 sample cross pattern
    vec3 crossMin = min(tc, min(ml, min(mc, min(mr, bc))));
    vec3 crossMax = max(tc, max(ml, max(mc, max(mr, bc))));

    // Average both bounding boxes to get a more rounded shape
    vec3 localNeighbourhoodMin = 0.5 * (boxMin + crossMin);
    vec3 localNeighbourhoodMax = 0.5 * (boxMax + crossMax);
    
    //ComputeVarianceMinMax(localNeighbourhoodMin, localNeighbourhoodMax);
    vec3 average = 0.5 * (localNeighbourhoodMin + localNeighbourhoodMax);

    ComputeVarianceMinMax(localNeighbourhoodMin, localNeighbourhoodMax);

    ivec2 velocityPixel = clamp(pixel + offset, ivec2(0), ivec2(PushConstants.resolution) - ivec2(1));
    vec2 velocity = texelFetch(velocityTexture, velocityPixel, 0).rg;
    vec2 lastVelocity = texelFetch(lastVelocityTexture, pixel, 0).rg;

    float velocityBlend = saturate(600.0 * max(length(velocity), length(lastVelocity)));

    vec2 uv = (vec2(pixel) + vec2(0.5)) * PushConstants.invResolution + velocity;

    // Maybe we might want to filter the current input pixel
    vec4 history = SampleHistory(uv);

    vec3 historyColor = history.rgb;
    vec3 currentColor = mc;

    float lumaHistory = Luma(historyColor);
    float lumaCurrent = Luma(currentColor);

    float historyContrast = saturate(abs(lumaCurrent - lumaHistory) / max3(vec3(0.2, lumaCurrent, lumaHistory)));
    float range = 1.4;
    float localAntiFlicker = mix(0.6, 5.0, 1.0 - velocityBlend);

#ifndef PATHTRACE
#ifdef TAA_DENOISE
    range += localAntiFlicker;
#else
    range += mix(0.0, localAntiFlicker, historyContrast);
#endif
#endif

    localNeighbourhoodMin = average - range * (average - localNeighbourhoodMin);
    localNeighbourhoodMax = average + range * (localNeighbourhoodMax - average);

    // Clamp/Clip the history to the localNeighbourhood bounding box
#ifdef TAA_CLIP
    float clipBlend = ClipBoundingBox(localNeighbourhoodMin, localNeighbourhoodMax,
        historyColor, currentColor);
    float adjClipBlend = saturate(clipBlend);
    historyColor = mix(historyColor, currentColor, adjClipBlend);
#else
    historyColor = clamp(historyColor, localNeighbourhoodMin, localNeighbourhoodMax);
#endif

#ifndef PATHTRACE
    float blendFactor = mix(minVelocityBlend, maxVelocityBlend, velocityBlend);
#else
    float blendFactor = mix(minVelocityBlend, maxVelocityBlend, velocityBlend);
#endif

#ifdef TAA_YCOCG
    historyColor = YCoCgToRGB(historyColor); 
    currentColor = YCoCgToRGB(currentColor);
#endif

#ifdef TAA_TONE
    historyColor.rgb = InverseTonemap(historyColor.rgb);
    currentColor.rgb = InverseTonemap(currentColor.rgb);
#endif

#ifndef PATHTRACE
    StencilFeatures features = DecodeStencilFeatures(texelFetch(stencilTexture, pixel, 0).r);
    blendFactor = features.responsivePixel ? 0.5 : blendFactor;
#endif    

    // Check if we sampled outside the viewport area
    blendFactor = (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0
         || uv.y > 1.0) ? 1.0 : blendFactor;

    const vec3 luma = vec3(0.299, 0.587, 0.114);
    float weightHistory = (1.0 - blendFactor) / (1.0 + dot(historyColor.rgb, luma));
    float weightCurrent =  blendFactor / (1.0 + dot(currentColor.rgb, luma));

    vec3 resolve = historyColor.rgb * weightHistory + 
        currentColor.rgb * weightCurrent;

    resolve /= (weightHistory + weightCurrent);

    // Some components might have a value of -0.0 which produces
    // errors later on while rendering (postprocessing saturate)
    resolve = abs(resolve);

    // Some stuff in the pipeline produces NaNs
    if (isnan(resolve.r) == true ||
        isnan(resolve.g) == true ||
        isnan(resolve.b) == true) {
        resolve = vec3(1, 0, 0);
    }

    imageStore(resolveImage, pixel, vec4(resolve, 0.0));

}