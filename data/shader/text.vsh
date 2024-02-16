#include <globals.hsh>

layout(location=0) out vec3 texCoordVS;
layout(location=1) out vec2 screenPositionVS;
#ifdef TEXT_3D
layout(location=2) out vec3 ndcCurrentVS;
layout(location=3) out vec3 ndcLastVS;
#endif

struct GlyphInfo {
    vec2 scale;
    vec2 size;
};

layout (set = 3, binding = 1, std430) buffer GlyphBuffer {
    GlyphInfo glyphs[];
};

layout (set = 3, binding = 2, std430) buffer InstancesBuffer {
    vec4 instances[];
};

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

    const vec3 positions[4] = vec3[4](
        vec3(1.f,1.f, 0.0f),
        vec3(1.f,-1.f, 0.0f),
        vec3(-1.f,1.f, 0.0f),
        vec3(-1.f,-1.f, 0.0f)
        );
    
    vec3 characterInfo = instances[gl_InstanceIndex].xyz;

    uint glyphIndex = uint(characterInfo.z);
    
    GlyphInfo glyph = glyphs[glyphIndex];

    vec2 vPosition = positions[gl_VertexIndex].xy;
    
    vec2 position = (vPosition + 1.0) / 2.0;
#ifdef TEXT_3D
    screenPositionVS = (position * glyph.size * pushConstants.textScale + characterInfo.xy);
#else
    screenPositionVS = position * glyph.size * pushConstants.textScale + 
        characterInfo.xy * pushConstants.textScale + pushConstants.textOffset;
#endif

    texCoordVS = vec3(position * glyph.scale, characterInfo.z);

#ifdef TEXT_3D
    vec3 right = normalize(pushConstants.right.xyz);
    vec3 down = normalize(pushConstants.down.xyz);

    vec2 halfSize = pushConstants.renderHalfSize;
    vec3 worldPosition = right * screenPositionVS.x + down * screenPositionVS.y;
    worldPosition = worldPosition + pushConstants.position.xyz - right * halfSize.x - down * halfSize.y;

    gl_Position = globalData.pMatrix * globalData.vMatrix * vec4(worldPosition, 1.0);

    ndcCurrentVS = vec3(gl_Position.xy, gl_Position.w);
    // For moving objects we need the last frames matrix
    vec4 last = globalData.pvMatrixLast * vec4(worldPosition, 1.0);
    ndcLastVS = vec3(last.xy, last.w);
#else
    vec2 clipPosition = 2.0 * (screenPositionVS / pushConstants.renderArea) - 1.0;
    gl_Position = vec4(clipPosition, 0.0, 1.0);
#endif
    
}