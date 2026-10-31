#include <../common/normalencode.hsh>
#include <../common/stencil.hsh>
#include <../globals.hsh>

#include <common.hsh>
#include <surface.hsh>

layout (location = 0) in vec3 fOriginalCoord;

layout (set = 3, binding = 5) uniform sampler2D depthTexture;

layout (location = 0) out uint stencil;
// Encoded view space normal, the ray origins for the reflections
layout (location = 1) out vec2 normal;

void main() {

    
    StencilFeatures features = CreateStencilFeatures();

    /*
    vec3 viewDir = normalize(fPosition - globalData.cameraLocation.xyz);
    bool frontFacing = dot(normalize(fNormal), viewDir) < 0.0 ? true : false;
    */

    if (!gl_FrontFacing) {
        features.underWaterPixel = true;
    }
    else {
        features.underWaterPixel = false;
    }

    features.waterPixel = true;

    // Only the visible surface needs a normal, the depth is still needed below the scene
    normal = vec2(0.0);
    if (gl_FragCoord.z <= texelFetch(depthTexture, ivec2(gl_FragCoord.xy), 0).r) {
        float fold, jacobian, slopeVariance;
        vec2 gradient;
        float viewDistance = distance(fOriginalCoord, globalData.cameraLocation.xyz);
        GetOceanGradientAndFold(fOriginalCoord.xz, viewDistance, fold, gradient, jacobian, slopeVariance);

        // The reflections treat the water surface as a perfect mirror
        vec3 worldNormal = normalize(vec3(gradient.x, 1.0, gradient.y));
        normal = EncodeNormal(normalize(vec3(globalData.vMatrix * vec4(worldNormal, 0.0))));
    }

    // Remember that the ocean stencil has only 8 bits, need to be careful about the features
    // and their bit order.
    stencil = EncodeStencilFeatures(features);
	

}