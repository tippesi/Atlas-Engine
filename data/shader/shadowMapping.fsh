#include <deferred/texture.hsh>

#ifdef OPACITY_MAP
layout(location = 0) in vec2 texCoordVS;
#endif

layout(push_constant) uniform constants {
    mat4 lightSpaceMatrix;
    uint vegetation;
    uint invertUVs;
    float windTextureLod;
    float windBendScale;
    float windWiggleScale;
    uint textureID;
} PushConstants;

void main() {
    
#ifdef OPACITY_MAP
    float opacity = SampleOpacity(texCoordVS, PushConstants.textureID, 0.0);
    if(opacity < 0.2)
        discard;
#endif
    
}