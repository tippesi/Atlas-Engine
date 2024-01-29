// Based on https://panoskarabelas.com/posts/screen_space_shadows/

#include <../globals.hsh>
#include <../ddgi/ddgi.hsh>
#include <../brdf/brdfEval.hsh>
#include <../common/flatten.hsh>
#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../common/normalencode.hsh>

#define TRACE_WORLD_SPACE

layout (local_size_x = 8, local_size_y = 8) in;

layout(set = 3, binding = 0, r16f) uniform image2D image;
layout(set = 3, binding = 1) uniform sampler2D depthTexture;
layout(set = 3, binding = 2) uniform sampler2D normalTexture;

layout(push_constant) uniform constants {
    vec4 lightDirection;
    int sampleCount;
    float maxLength;
    float thickness;
} pushConstants;

// https://blog.demofox.org/2022/01/01/interleaved-gradient-noise-a-different-kind-of-low-discrepancy-sequence/
float GetInterleavedGradientNoise(vec2 screenPos) {
    uint frame = globalData.frameCount % 64u;
    float x = float(screenPos.x) + 5.588238 * float(frame);
    float y = float(screenPos.y) + 5.588238 * float(frame);

    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(vec2(x, y), magic.xy)));
}

float EdgeFadeOut(vec2 screenPos, float fadeDist) {

    screenPos = abs(screenPos * 2.0 - 1.0);
    vec2 fadeOut = (screenPos - 1.0 + fadeDist) / fadeDist;
    return saturate(1.0 - max(fadeOut.x, fadeOut.y));

}

vec2 PosToUV(vec3 pos) {

    vec4 clipSpace = globalData.pMatrix * vec4(pos, 1.0);
    clipSpace.xyz /= clipSpace.w;
    return clipSpace.xy * 0.5 + 0.5;

}

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    ivec2 resolution = imageSize(image);

    if (pixel.x > resolution.x ||
        pixel.y > resolution.y)
        return;

    float depth = texelFetch(depthTexture, pixel, 0).r;
    vec3 normal = DecodeNormal(texelFetch(normalTexture, pixel, 0).rg);
    
    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(resolution);

    vec3 rayPos = ConvertDepthToViewSpace(depth, texCoord);
    float startDepth = -rayPos.z;
    vec3 rayDir = normalize(-pushConstants.lightDirection.xyz);

    float stepLength = pushConstants.maxLength / (float(pushConstants.sampleCount));

#ifdef TRACE_WORLD_SPACE
    float depthThickness = pushConstants.thickness;
    vec3 rayEndPos = rayPos + pushConstants.maxLength * rayDir;
#else
    float depthThickness = pushConstants.thickness;
    vec2 stepPos = PosToUV(rayPos + rayDir);
    float stepPosLength = length(stepPos - texCoord);
    vec3 rayEndPos = rayPos + (pushConstants.maxLength / max(0.01, stepPosLength)) * rayDir;
    depthThickness *= stepPosLength;
    depthThickness = max(abs(rayPos.z), abs(rayPos.z - rayEndPos.z)) * pushConstants.thickness;
    vec2 uvDir = normalize(stepPos - texCoord);
#endif

    float resultDelta = 0.0;
    vec2 resultUV = texCoord;
    int resultStep = -1;
    vec3 resultPos = rayPos;
    float resultDepth = -rayPos.z;

    float rayLength = distance(rayPos, rayEndPos);

    //depthThickness = abs(rayPos.z - rayEndPos.z) * stepLength;

    vec2 uvPos = texCoord;
    float noiseOffset = GetInterleavedGradientNoise(texCoord * vec2(resolution));

#ifdef TRACE_WORLD_SPACE
    rayPos += noiseOffset * stepLength * rayDir;
#else
    uvPos += noiseOffset * stepLength * uvDir;
    rayPos += noiseOffset * stepLength * rayDir / stepPosLength;
#endif

    // Ray march towards the light
    float occlusion = 0.0;
    for (int i = 0; i < pushConstants.sampleCount; i++) {
        // Step the ray
#ifdef TRACE_WORLD_SPACE
        rayPos += rayDir * stepLength;
        
        vec4 offset = globalData.pMatrix * vec4(rayPos, 1.0);
        offset.xyz /= offset.w;
        uvPos = offset.xy * 0.5 + 0.5;
#else
        uvPos += uvDir * stepLength;
        rayPos += rayDir * stepLength / stepPosLength;
#endif
        if (uvPos.x < 0.0 || uvPos.x > 1.0 || uvPos.y < 0.0 || uvPos.y > 1.0)
            continue;

        ivec2 stepPixel = ivec2(uvPos * vec2(resolution));
        float stepDepth = texelFetch(depthTexture, stepPixel, 0).r;

        if (stepDepth == depth) {
            continue;
        }

        float stepLinearDepth = -ConvertDepthToViewSpaceDepth(stepDepth);
        float rayDepth = -rayPos.z;

        float depthDelta = rayDepth - stepLinearDepth;

        // Check if the camera can't "see" the ray (ray depth must be larger than the camera depth, so positive depth_delta)
        if (depthDelta > 0.0 && depthDelta < depthThickness &&
            depthDelta < 0.5 * rayLength && depthDelta > 0.2 * depthThickness) {
            // Mark as occluded
            occlusion = 1.0;
            resultDelta = depthDelta;
            resultUV = uvPos;
            resultStep = i;
            resultPos = rayPos;
            resultDepth = stepLinearDepth;

            // Fade out as we approach the edges of the screen
            occlusion *= EdgeFadeOut(uvPos, 0.005);

            break;
        }
    }

    imageStore(image, pixel, vec4(1.0 - occlusion, 0.0,0.0,0.0));

}