#include <../globals.hsh>
#include <../common/convert.hsh>
#include <../common/stencil.hsh>

// The G-buffer, but the reflections start on the ocean surface where it's visible

layout (local_size_x = 8, local_size_y = 8) in;

layout (set = 3, binding = 0) uniform sampler2D depthIn;
layout (set = 3, binding = 1) uniform sampler2D normalIn;
layout (set = 3, binding = 2) uniform sampler2D geometryNormalIn;
layout (set = 3, binding = 3) uniform sampler2D roughnessMetallicAoIn;
layout (set = 3, binding = 4) uniform sampler2D velocityIn;
layout (set = 3, binding = 5) uniform usampler2D materialIdxIn;

layout (set = 3, binding = 6) uniform sampler2D oceanDepthIn;
layout (set = 3, binding = 7) uniform sampler2D oceanNormalIn;
layout (set = 3, binding = 8) uniform usampler2D oceanStencilIn;

layout (set = 3, binding = 9, r32f) writeonly uniform image2D depthOut;
layout (set = 3, binding = 10, rg16f) writeonly uniform image2D normalOut;
layout (set = 3, binding = 11, rg16f) writeonly uniform image2D geometryNormalOut;
layout (set = 3, binding = 12, rgba8) writeonly uniform image2D roughnessMetallicAoOut;
layout (set = 3, binding = 13, rg16f) writeonly uniform image2D velocityOut;
layout (set = 3, binding = 14, r16ui) writeonly uniform uimage2D materialIdxOut;

// Like the G-buffer downscale, the half resolution data is the top left pixel of each 2x2 block
layout (set = 3, binding = 15, r32f) writeonly uniform image2D halfResDepthOut;
layout (set = 3, binding = 16, rg16f) writeonly uniform image2D halfResNormalOut;
layout (set = 3, binding = 17, rg16f) writeonly uniform image2D halfResGeometryNormalOut;
layout (set = 3, binding = 18, rgba8) writeonly uniform image2D halfResRoughnessMetallicAoOut;
layout (set = 3, binding = 19, rg16f) writeonly uniform image2D halfResVelocityOut;
layout (set = 3, binding = 20, r16ui) writeonly uniform uimage2D halfResMaterialIdxOut;
layout (set = 3, binding = 21, r8i) writeonly uniform iimage2D halfResOffsetOut;

layout(push_constant) uniform constants {
    uint oceanMaterialIdx;
} PushConstants;

void Store(ivec2 pixel, float depth, vec2 normal, vec2 geometryNormal, vec4 roughnessMetallicAo,
    vec2 velocity, uint materialIdx) {

    imageStore(depthOut, pixel, vec4(depth));
    imageStore(normalOut, pixel, vec4(normal, 0.0, 0.0));
    imageStore(geometryNormalOut, pixel, vec4(geometryNormal, 0.0, 0.0));
    imageStore(roughnessMetallicAoOut, pixel, roughnessMetallicAo);
    imageStore(velocityOut, pixel, vec4(velocity, 0.0, 0.0));
    imageStore(materialIdxOut, pixel, uvec4(materialIdx));

    ivec2 halfResPixel = pixel / 2;
    if (pixel.x % 2 != 0 || pixel.y % 2 != 0 || any(greaterThanEqual(halfResPixel, imageSize(halfResDepthOut))))
        return;

    imageStore(halfResDepthOut, halfResPixel, vec4(depth));
    imageStore(halfResNormalOut, halfResPixel, vec4(normal, 0.0, 0.0));
    imageStore(halfResGeometryNormalOut, halfResPixel, vec4(geometryNormal, 0.0, 0.0));
    imageStore(halfResRoughnessMetallicAoOut, halfResPixel, roughnessMetallicAo);
    imageStore(halfResVelocityOut, halfResPixel, vec4(velocity, 0.0, 0.0));
    imageStore(halfResMaterialIdxOut, halfResPixel, uvec4(materialIdx));
    imageStore(halfResOffsetOut, halfResPixel, ivec4(0));

}

void main() {

    ivec2 resolution = imageSize(depthOut);
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    if (pixel.x >= resolution.x || pixel.y >= resolution.y)
        return;

    float depth = texelFetch(depthIn, pixel, 0).r;
    float oceanDepth = texelFetch(oceanDepthIn, pixel, 0).r;
    StencilFeatures features = DecodeStencilFeatures(texelFetch(oceanStencilIn, pixel, 0).r);

    if (oceanDepth >= 1.0 || oceanDepth > depth || features.underWaterPixel) {
        Store(pixel, depth, texelFetch(normalIn, pixel, 0).rg, texelFetch(geometryNormalIn, pixel, 0).rg,
            texelFetch(roughnessMetallicAoIn, pixel, 0), texelFetch(velocityIn, pixel, 0).rg,
            texelFetch(materialIdxIn, pixel, 0).r);
        return;
    }

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(resolution);
    vec3 viewPos = ConvertDepthToViewSpace(oceanDepth, texCoord);
    vec3 worldPos = vec3(globalData.ivMatrix * vec4(viewPos, 1.0));

    vec2 normal = texelFetch(oceanNormalIn, pixel, 0).rg;

    // Same velocity as the ocean surface pass, which only accounts for the camera movement
    vec4 last = globalData.pvMatrixLast * vec4(worldPos, 1.0);
    vec4 current = globalData.pMatrix * vec4(viewPos, 1.0);
    vec2 ndcLast = last.xy / last.w - globalData.jitterLast;
    vec2 ndcCurrent = current.xy / current.w - globalData.jitterCurrent;

    Store(pixel, oceanDepth, normal, normal, vec4(0.0, 0.0, 1.0, 0.0), (ndcLast - ndcCurrent) * 0.5,
        PushConstants.oceanMaterialIdx);

}
