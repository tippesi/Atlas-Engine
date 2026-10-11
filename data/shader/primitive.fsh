#include <globals.hsh>
#include <common/stencil.hsh>

layout(location=0) out vec3 colorFS;
layout(location=1) out vec2 velocityFS;
layout(location=2) out uint stencil;

layout(location=0) in vec3 colorVS;
layout(location=1) in vec3 ndcCurrentVS;
layout(location=2) in vec3 ndcLastVS;

#ifdef MANUAL_DEPTH_TEST
// When the scene is path traced there is no depth attachment, the path tracer writes the depth into this texture
layout(set = 3, binding = 0) uniform sampler2D depthTexture;
#endif

void main() {

#ifdef MANUAL_DEPTH_TEST
    // Path traced depth is the same projected z / w as gl_FragCoord.z, misses are written as infinity
    float sceneDepth = texelFetch(depthTexture, ivec2(gl_FragCoord.xy), 0).r;
    if (gl_FragCoord.z > sceneDepth + 1e-5)
        discard;
#endif

    colorFS = colorVS;

    vec2 ndcL = ndcLastVS.xy / ndcLastVS.z;
    vec2 ndcC = ndcCurrentVS.xy / ndcCurrentVS.z;

    ndcL -= globalData.jitterLast;
    ndcC -= globalData.jitterCurrent;

    velocityFS = (ndcL - ndcC) * 0.5;

    StencilFeatures features = CreateStencilFeatures();
    features.responsivePixel = true;
    stencil = EncodeStencilFeatures(features);

}