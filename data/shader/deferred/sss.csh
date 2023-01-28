// Based on https://panoskarabelas.com/posts/screen_space_shadows/

#include <../globals.hsh>
#include <../ddgi/ddgi.hsh>
#include <../brdf/brdfEval.hsh>
#include <../common/flatten.hsh>
#include <../common/convert.hsh>
#include <../common/utility.hsh>

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
    vec3 normal = 2.0 * texelFetch(normalTexture, pixel, 0).rgb - 1.0;
    
    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(resolution);

    vec3 rayPos = ConvertDepthToViewSpace(depth, texCoord);
    float startDepth = -rayPos.z;
    vec3 rayDir = normalize(-pushConstants.lightDirection.xyz);

    vec3 rayEndPos = rayPos + pushConstants.maxLength;
    float endDepth = textureLod(depthTexture, PosToUV(rayEndPos), 0.0).r;
    float rayEndDepth = ConvertDepthToViewSpaceDepth(endDepth);

    // Compute ray step
    float depthMultiplier = max(10.0, -rayPos.z);
    float stepLength = pushConstants.maxLength / (float(pushConstants.sampleCount));
    float depthThickness = 0.25 * pushConstants.maxLength;

    float resultDelta = 0.0;
    vec2 resultUV = texCoord;
    int resultStep = -1;
    vec3 resultPos = rayPos;
    float resultDepth = -rayPos.z;

    //depthThickness = abs(rayPos.z - rayEndPos.z) * stepLength;

    float noiseOffset = GetInterleavedGradientNoise(texCoord * vec2(resolution));
    rayPos += noiseOffset * stepLength * rayDir;

    // Ray march towards the light
    float occlusion = 0.0;
    for (int i = 0; i < pushConstants.sampleCount; i++) {
        // Step the ray
        rayPos += rayDir * stepLength;
        
        vec4 offset = globalData.pMatrix * vec4(rayPos, 1.0);
        offset.xyz /= offset.w;
        vec2 uvPos = offset.xy * 0.5 + 0.5;

        ivec2 stepPixel = ivec2(uvPos * vec2(resolution));
        float stepDepth = texelFetch(depthTexture, stepPixel, 0).r;

        if (stepDepth == depth) {
            continue;
        }

        float stepLinearDepth = -ConvertDepthToViewSpaceDepth(stepDepth);
        float rayDepth = -rayPos.z;

        float depthDelta = rayDepth - stepLinearDepth;

        // Check if the camera can't "see" the ray (ray depth must be larger than the camera depth, so positive depth_delta)
        if (depthDelta > 0.0 && depthDelta < pushConstants.thickness && 
            depthDelta > pushConstants.thickness * 0.25) {
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
    //imageStore(image, pixel, vec4(float(resultStep) / 2, resultDelta, resultUV - texCoord));
    // imageStore(image, pixel, vec4(float(resultStep) / 2, 0.0,0.0,0.0));
    // imageStore(image, pixel, vec4(float(resultDelta) * 1000.0, 0.0,0.0,0.0));
    //imageStore(image, pixel, vec4(rayDir * 0.5 + 0.5,0.0));

}