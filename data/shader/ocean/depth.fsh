#include <../common/normalencode.hsh>
#include <../common/stencil.hsh>
#include <../globals.hsh>

layout (location = 0) out uint stencil;

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

    // Remember that the ocean stencil has only 8 bits, need to be careful about the features
    // and their bit order.
    stencil = EncodeStencilFeatures(features);
	

}