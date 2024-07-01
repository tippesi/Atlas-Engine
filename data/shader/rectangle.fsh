layout(location=0) out vec4 colorFS;

layout(location=0) in vec2 screenPositionVS;

#if defined(TEXTURE2D) || defined(TEXTURE2D_ARRAY) || defined(TEXTURE3D)
layout(location=1) in vec2 texCoordVS;
#endif

#ifdef TEXTURE2D
layout(set = 3, binding = 0) uniform sampler2D rectangleTexture;
#elif defined(TEXTURE2D_ARRAY)
layout(set = 3, binding = 0) uniform sampler2DArray rectangleTexture;
#elif defined(TEXTURE3D)
layout(set = 3, binding = 0) uniform sampler3D rectangleTexture;
#endif

layout(push_constant) uniform constants {
    mat4 pMatrix;
    vec4 blendArea;
    vec4 clipArea;
    vec2 offset;
    vec2 scale;
    int invert;
    float depth;
    float rotation;
    float brightness;
} pushConstants;

void main() {

    if (screenPositionVS.x < pushConstants.clipArea.x ||
        screenPositionVS.y < pushConstants.clipArea.y ||
        screenPositionVS.x > pushConstants.clipArea.x + pushConstants.clipArea.z ||
        screenPositionVS.y > pushConstants.clipArea.y + pushConstants.clipArea.w)
        discard;

#if defined(TEXTURE2D) || defined(TEXTURE2D_ARRAY) || defined(TEXTURE3D)
    vec2 texCoord = texCoordVS;

    if (pushConstants.invert > 0) {
        texCoord.y = 1.0 - texCoord.y;
    }
#endif
    
#ifdef TEXTURE2D
    colorFS = textureLod(rectangleTexture, vec2(texCoord), 0.0);
#elif defined(TEXTURE2D_ARRAY)
    colorFS = textureLod(rectangleTexture, vec3(texCoord, pushConstants.depth), 0.0);
#elif defined(TEXTURE3D)
    colorFS = textureLod(rectangleTexture, vec3(texCoord, pushConstants.depth), 0.0);
#else
    colorFS = rectangleColor;
#endif

#ifndef ALPHA_BLENDING
    colorFS.a = 1.0;
#endif

    colorFS *= pushConstants.brightness;

}