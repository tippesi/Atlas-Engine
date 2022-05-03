#include <common/utility.hsh>
#include <common/PI.hsh>
#include <common/stencil.hsh>

layout(location = 0) out vec3 result;

layout(binding = 0) uniform sampler2D historyTexture;
layout(binding = 1) uniform sampler2D currentTexture;
layout(binding = 2) uniform sampler2D velocityTexture;
layout(binding = 3) uniform sampler2D depthTexture;
layout(binding = 4) uniform sampler2D lastVelocityTexture;
layout(binding = 5) uniform usampler2D stencilTexture;

in vec2 fTexCoord;

uniform vec2 invResolution;
uniform vec2 resolution;

uniform vec2 jitter;

uniform float minVelocityBlend = 0.05;
uniform float maxVelocityBlend = 0.8;

#define TAA_YCOCG
//#define TAA_CLIP // Use clip instead of clamping for better ghosting prevention, introduces more flickering
#define TAA_BICUBIC // Nearly always use the bicubic sampling for better quality and sharpness under movement
#define TAA_TONE // Somehow introduces more flickering as well

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

vec3 neighbourhood[9];

const mat3 RGBToYCoCgMatrix = mat3(0.25, 0.5, -0.25, 0.5, 0.0, 0.5, 0.25, -0.5, -0.25);
const mat3 YCoCgToRGBMatrix = mat3(1.0, 1.0, 1.0, 1.0, 0.0, -1.0, -1.0, 1.0, -1.0);

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

    texel = clamp(texel, ivec2(0), ivec2(resolution) - ivec2(1));
	
	vec3 color = max(texelFetch(currentTexture, texel, 0).rgb, 0);

#ifdef TAA_TONE
	color = Tonemap(color);
#endif

#ifdef TAA_YCOCG
    color = RGBToYCoCg(color);
#endif

	return color;

}

void SampleNeighbourhood(ivec2 pixel) {

    neighbourhood[0] = FetchTexel(pixel + offsets[0]);
    neighbourhood[1] = FetchTexel(pixel + offsets[1]);
    neighbourhood[2] = FetchTexel(pixel + offsets[2]);
    neighbourhood[3] = FetchTexel(pixel + offsets[3]);
    neighbourhood[4] = FetchTexel(pixel + offsets[4]);
    neighbourhood[5] = FetchTexel(pixel + offsets[5]);
    neighbourhood[6] = FetchTexel(pixel + offsets[6]);
    neighbourhood[7] = FetchTexel(pixel + offsets[7]);
    neighbourhood[8] = FetchTexel(pixel + offsets[8]);

}

ivec2 FindNearest3x3(ivec2 pixel) {

    ivec2 offset = ivec2(0);
    float depth = 1.0;

    for (int i = 0; i < 9; i++) {
        ivec2 pixelCoord = pixel + offsets[i];

        float currDepth = texelFetch(depthTexture, pixelCoord, 0).r;
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
        vec3 color = InverseTonemap(YCoCgToRGB(neighbourhood[i]));
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
        vec3 color = neighbourhood[i];
        
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

    ivec2 pixel = ivec2(fTexCoord * resolution);

    // Find nearest pixel in neighbourhood to improve velocity sampling
    ivec2 offset = FindNearest3x3(pixel);

    SampleNeighbourhood(pixel);

    vec3 tl = neighbourhood[0];
    vec3 tc = neighbourhood[1];
    vec3 tr = neighbourhood[2];
    vec3 ml = neighbourhood[3];
    vec3 mc = neighbourhood[4];
    vec3 mr = neighbourhood[5];
    vec3 bl = neighbourhood[6];
    vec3 bc = neighbourhood[7];
    vec3 br = neighbourhood[8];

    // 3x3 box pattern
    vec3 boxMin = min(tl, min(tc, min(tr, min(ml, min(mc, min(mr, min(bl, min(bc, br))))))));
    vec3 boxMax = max(tl, max(tc, max(tr, max(ml, max(mc, max(mr, max(bl, max(bc, br))))))));

    // 5 sample cross pattern
    vec3 crossMin = min(tc, min(ml, min(mc, min(mr, bc))));
	vec3 crossMax = max(tc, max(ml, max(mc, max(mr, bc))));

    // Average both bounding boxes to get a more rounded shape
    vec3 neighbourhoodMin = 0.5 * (boxMin + crossMin);
    vec3 neighbourhoodMax = 0.5 * (boxMax + crossMax);
    
    //ComputeVarianceMinMax(neighbourhoodMin, neighbourhoodMax);
    vec3 average = 0.5 * (neighbourhoodMin + neighbourhoodMax);

    ComputeVarianceMinMax(neighbourhoodMin, neighbourhoodMax);

    ivec2 velocityPixel = clamp(pixel + offset, ivec2(0), ivec2(resolution) - ivec2(1));
    vec2 velocity = texelFetch(velocityTexture, velocityPixel, 0).rg;
    vec2 uv = (vec2(pixel) + vec2(0.5)) * invResolution + velocity;

    // Maybe we might want to filter the current input pixel
    vec4 history = SampleHistory(uv);

    vec3 historyColor = history.rgb;
    vec3 currentColor = SampleCurrent();

    float lumaHistory = Luma(historyColor);
    float lumaCurrent = Luma(currentColor);

    float historyContrast = saturate(abs(lumaCurrent - lumaHistory) / max3(vec3(0.2, lumaCurrent, lumaHistory)));
    float range = 1.4;
    float localAntiFlicker = mix(0.6, 3.0, saturate(1.0 - 1000.0 * length(velocity)));
    range += mix(0.0, localAntiFlicker, historyContrast);
    neighbourhoodMin = average - range * (average - neighbourhoodMin);
    neighbourhoodMax = average + range * (neighbourhoodMax - average);

    float lumaMin = Luma(neighbourhoodMin);
    float lumaMax = Luma(neighbourhoodMax);
    vec2 lastVelocity = texelFetch(lastVelocityTexture, velocityPixel, 0).rg;

    // Clamp/Clip the history to the neighbourhood bounding box
#ifdef TAA_CLIP
    float clipBlend = ClipBoundingBox(neighbourhoodMin, neighbourhoodMax,
        historyColor, currentColor);
    float adjClipBlend = saturate(clipBlend);
    historyColor = mix(historyColor, currentColor, adjClipBlend);
#else
    historyColor = clamp(historyColor, neighbourhoodMin, neighbourhoodMax);
#endif

    // Track subpixel velocity movement
    float lastCorrection = fract(max(abs(lastVelocity.x) * resolution.x,
		abs(lastVelocity.y) * resolution.y)) * 0.5;
	float correction = fract(max(abs(velocity.x) * resolution.x,
		abs(velocity.y) * resolution.y)) * 0.5;
    correction = saturate(max(correction, lastCorrection));
    float blendFactor = mix(minVelocityBlend, maxVelocityBlend, correction);

	// Check if we sampled outside the viewport area
    blendFactor = (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0
         || uv.y > 1.0) ? 1.0 : blendFactor;

#ifdef TAA_YCOCG
    historyColor = YCoCgToRGB(historyColor); 
    currentColor = YCoCgToRGB(currentColor);
#endif

#ifdef TAA_TONE
	historyColor.rgb = InverseTonemap(historyColor.rgb);
    currentColor.rgb = InverseTonemap(currentColor.rgb);
#endif

    StencilFeatures features = DecodeStencilFeatures(texelFetch(stencilTexture, pixel, 0).r);
    blendFactor = features.responsivePixel ? 0.5 : blendFactor;

    const vec3 luma = vec3(0.299, 0.587, 0.114);
    float weightHistory = (1.0 - blendFactor) / (1.0 + dot(historyColor.rgb, luma));
    float weightCurrent =  blendFactor / (1.0 + dot(currentColor.rgb, luma));

    result.rgb = historyColor.rgb * weightHistory + 
        currentColor.rgb * weightCurrent;

    result.rgb /= (weightHistory + weightCurrent);

    // Some components might have a value of -0.0 which produces
    // errors later on while rendering (postprocessing saturate)
    result.rgb = abs(result.rgb);

    // Some stuff in the pipeline produces NaNs
    if (isnan(result.r) == true ||
        isnan(result.g) == true ||
        isnan(result.b) == true) {
        result.rgb = vec3(1, 0, 0);
    }

}