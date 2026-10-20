// Hierarchical depth with the closest depth in r and the farthest depth plus the thickness in g.
// Each work group reduces a 32x32 block of the input and writes up to six levels.

#include <../globals.hsh>

layout (local_size_x = 16, local_size_y = 16) in;

layout(set = 3, binding = 0) uniform sampler2D depthIn;
layout(set = 3, binding = 1, rg32f) readonly uniform image2D levelIn;

layout(set = 3, binding = 2, rg32f) writeonly uniform image2D levelOut0;
layout(set = 3, binding = 3, rg32f) writeonly uniform image2D levelOut1;
layout(set = 3, binding = 4, rg32f) writeonly uniform image2D levelOut2;
layout(set = 3, binding = 5, rg32f) writeonly uniform image2D levelOut3;
layout(set = 3, binding = 6, rg32f) writeonly uniform image2D levelOut4;
layout(set = 3, binding = 7, rg32f) writeonly uniform image2D levelOut5;

layout(push_constant) uniform constants {
    ivec2 inputSize;
    int fromDepth;
    int levelCount;
    float thickness;
} pushConstants;

shared vec2 sharedMinMax[16][16];

vec2 FetchTexel(ivec2 texel) {

    if (any(greaterThanEqual(texel, pushConstants.inputSize)))
        return vec2(1.0);

    if (pushConstants.fromDepth > 0) {
        float depth = texelFetch(depthIn, texel, 0).r;
        if (depth >= 1.0 || pushConstants.thickness <= 0.0)
            return vec2(depth);

        vec2 viewSpace = (globalData.ipMatrix * vec4(0.0, 0.0, depth, 1.0)).zw;
        float viewZ = viewSpace.x / viewSpace.y;
        viewZ -= pushConstants.thickness * mix(0.0, 4.0, min(1.0, -viewZ * 0.002));
        vec4 clipSpace = globalData.pMatrix * vec4(0.0, 0.0, viewZ, 1.0);
        return vec2(depth, clipSpace.z / clipSpace.w);
    }

    return imageLoad(levelIn, texel).rg;

}

vec2 Combine(vec2 a, vec2 b) {

    return vec2(min(a.x, b.x), max(a.y, b.y));

}

void Store(int level, ivec2 texel, vec2 value) {

    if (level >= pushConstants.levelCount)
        return;

    if (level == 0) imageStore(levelOut0, texel, vec4(value, 0.0, 0.0));
    else if (level == 1) imageStore(levelOut1, texel, vec4(value, 0.0, 0.0));
    else if (level == 2) imageStore(levelOut2, texel, vec4(value, 0.0, 0.0));
    else if (level == 3) imageStore(levelOut3, texel, vec4(value, 0.0, 0.0));
    else if (level == 4) imageStore(levelOut4, texel, vec4(value, 0.0, 0.0));
    else if (level == 5) imageStore(levelOut5, texel, vec4(value, 0.0, 0.0));

}

void main() {

    ivec2 pixel = ivec2(gl_LocalInvocationID);
    ivec2 workGroupOffset = ivec2(gl_WorkGroupID) * ivec2(gl_WorkGroupSize) * 2;

    // When reading the depth texture, the input is level 0
    ivec2 coord = workGroupOffset + 2 * pixel;
    vec2 depth00 = FetchTexel(coord);
    vec2 depth10 = FetchTexel(coord + ivec2(1, 0));
    vec2 depth01 = FetchTexel(coord + ivec2(0, 1));
    vec2 depth11 = FetchTexel(coord + ivec2(1, 1));

    int level = 0;
    if (pushConstants.fromDepth > 0) {
        Store(0, coord, depth00);
        Store(0, coord + ivec2(1, 0), depth10);
        Store(0, coord + ivec2(0, 1), depth01);
        Store(0, coord + ivec2(1, 1), depth11);
        level = 1;
    }

    vec2 value = Combine(Combine(depth00, depth10), Combine(depth01, depth11));
    Store(level, ivec2(gl_WorkGroupID.xy) * 16 + pixel, value);
    sharedMinMax[pixel.y][pixel.x] = value;

    int size = 16;
    for (int i = 1; i < 5; i++) {
        barrier();

        size /= 2;
        if (all(lessThan(pixel, ivec2(size)))) {
            ivec2 texel = 2 * pixel;
            value = Combine(Combine(sharedMinMax[texel.y][texel.x], sharedMinMax[texel.y][texel.x + 1]),
                Combine(sharedMinMax[texel.y + 1][texel.x], sharedMinMax[texel.y + 1][texel.x + 1]));
            Store(level + i, ivec2(gl_WorkGroupID.xy) * size + pixel, value);
        }

        barrier();

        if (all(lessThan(pixel, ivec2(size))))
            sharedMinMax[pixel.y][pixel.x] = value;
    }

}
