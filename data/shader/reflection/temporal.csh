#include <../common/utility.hsh>
#include <../common/PI.hsh>
#include <../common/stencil.hsh>
#include <../common/flatten.hsh>
#include <../common/material.hsh>
#include <../common/random.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(binding = 0, rgba16f) writeonly uniform image2D resolveImage;

layout(binding = 0) uniform sampler2D historyTexture;
layout(binding = 1) uniform sampler2D currentTexture;
layout(binding = 2) uniform sampler2D velocityTexture;
layout(binding = 3) uniform sampler2D depthTexture;
layout(binding = 4) uniform sampler2D roughnessMetallicAoTexture;
layout(binding = 5) uniform isampler2D offsetTexture;
layout(binding = 6) uniform usampler2D materialIdxTexture;

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

vec3 localNeighbourhood[9];

vec3 FetchTexel(ivec2 texel) {
	
	vec3 color = max(texelFetch(currentTexture, texel, 0).rgb, 0);
	return color;

}

void LoadData(ivec2 pixel) {

    for (uint i = 0; i < 9; i++) {
        
        localNeighbourhood[i] = FetchTexel(pixel + offsets[i]);
    }

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

vec4 SampleHistory(vec2 texCoord) {

    vec4 historyColor;

    historyColor = texture(historyTexture, texCoord);

    return historyColor;

}

void ComputeVarianceMinMax(out vec3 aabbMin, out vec3 aabbMax) {

    vec3 m1 = vec3(0.0);
    vec3 m2 = vec3(0.0);
    const int radius = 4;
    ivec2 pixel = ivec2(gl_GlobalInvocationID);

    for (int i = -radius; i <= radius; i++) {
        for (int j = -radius; j <= radius; j++) {
            vec3 color = texelFetch(currentTexture, pixel + ivec2(i, j), 0).rgb;
        
            m1 += color;
            m2 += color * color;
        }
    }

    float oneDividedBySampleCount = 1.0 / float((2.0 * radius + 1.0) * (2.0 * radius + 1.0));
    float gamma = 1.0;
    vec3 mu = m1 * oneDividedBySampleCount;
    vec3 sigma = sqrt(abs((m2 * oneDividedBySampleCount) - (mu * mu)));
    aabbMin = mu - gamma * sigma;
    aabbMax = mu + gamma * sigma;

}

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(resolveImage).x ||
        pixel.y > imageSize(resolveImage).y)
        return;

    LoadData(pixel);

    // Find nearest pixel in localNeighbourhood to improve velocity sampling
    ivec2 offset = FindNearest3x3(pixel);

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

    ivec2 velocityPixel = pixel;
    vec2 velocity = texelFetch(velocityTexture, velocityPixel, 0).rg;

    vec2 uv = (vec2(pixel) + vec2(0.5)) * invResolution + velocity;

    // Maybe we might want to filter the current input pixel
    vec4 history = SampleHistory(uv);

    vec3 historyColor = history.rgb;
    vec3 currentColor = mc;

    localNeighbourhoodMin = average - (average - localNeighbourhoodMin);
    localNeighbourhoodMax = average + (localNeighbourhoodMax - average);

    float clipBlend = ClipBoundingBox(localNeighbourhoodMin, localNeighbourhoodMax,
        historyColor, currentColor);
    float adjClipBlend = saturate(clipBlend);
    historyColor = mix(historyColor, currentColor, adjClipBlend);

    int offsetIdx = texelFetch(offsetTexture, pixel, 0).r;
    ivec2 pixelOffset = pixelOffsets[offsetIdx];

    uint materialIdx = texelFetch(materialIdxTexture, pixel * 2 + pixelOffset, 0).r;
	Material material = UnpackMaterial(materialIdx);

    float roughness = texelFetch(roughnessMetallicAoTexture, pixel, 0).r;
    material.roughness *= material.roughnessMap ? roughness : 1.0;

    float factor = clamp(4.0 * material.roughness, 0.0, 0.95);
    factor = (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0
         || uv.y > 1.0) ? 0.0 : factor;

    vec3 resolve = mix(currentColor, historyColor, factor);

    imageStore(resolveImage, pixel, vec4(resolve, 0.0));

}