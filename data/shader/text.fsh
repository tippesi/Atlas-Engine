#include <globals.hsh>

layout (location=0) out vec4 colorFS;

#ifdef TEXT_3D
layout (location=1) out vec2 velocityFS;
#endif

layout(location=0) in vec3 texCoordVS;
layout(location=1) in vec2 screenPositionVS;
#ifdef TEXT_3D
layout(location=2) in vec3 ndcCurrentVS;
layout(location=3) in vec3 ndcLastVS;
#endif

layout(set = 3, binding = 0) uniform sampler2DArray glyphsTexture;

#ifdef TEXT_3D
layout(push_constant) uniform constants {
    vec4 position;
    vec4 right;
    vec4 down;
    vec4 characterColor;
    vec4 outlineColor;
    vec2 renderHalfSize;
    float textScale;
    float outlineScale;
    float edgeValue;
    float smoothness;
} pushConstants;
#else
layout(push_constant) uniform constants {
    vec4 clipArea;
    vec4 blendArea;
    vec4 characterColor;
    vec4 outlineColor;
    vec2 textOffset;
    vec2 renderArea;
    float textScale;
    float outlineScale;
    float edgeValue;
    float smoothness;
} pushConstants;
#endif

void main() {

    colorFS = vec4(0.0);

#ifndef TEXT_3D
    if (screenPositionVS.x < pushConstants.clipArea.x ||
        screenPositionVS.y < pushConstants.clipArea.y ||
        screenPositionVS.x > pushConstants.clipArea.x + pushConstants.clipArea.z ||
        screenPositionVS.y > pushConstants.clipArea.y + pushConstants.clipArea.w)
        discard;
#endif
    
    float intensity = textureLod(glyphsTexture, texCoordVS, 0).r;
    float smoothing = pushConstants.smoothness * fwidth(intensity);
    
    float outlineFactor = smoothstep(pushConstants.edgeValue - smoothing, 
        pushConstants.edgeValue + smoothing, intensity);
    colorFS = mix(pushConstants.outlineColor, pushConstants.characterColor, outlineFactor);
    
    float mixDistance = mix(pushConstants.edgeValue, 0.0, pushConstants.outlineScale);
    float alpha = smoothstep(mixDistance - smoothing, mixDistance + smoothing, intensity);
        
    colorFS = pushConstants.outlineScale > 0.0 ? vec4(colorFS.rgb, colorFS.a * alpha) : 
        vec4(pushConstants.characterColor.rgb, pushConstants.characterColor.a * alpha);

#ifdef TEXT_3D
    if (colorFS.a < 0.5)
        discard;

    vec2 ndcL = ndcLastVS.xy / ndcLastVS.z;
    vec2 ndcC = ndcCurrentVS.xy / ndcCurrentVS.z;

    ndcL -= globalData.jitterLast;
    ndcC -= globalData.jitterCurrent;

    velocityFS = (ndcL - ndcC) * 0.5;
#endif

}