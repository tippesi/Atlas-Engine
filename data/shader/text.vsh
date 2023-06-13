layout(location=0) in vec2 vPosition;

layout(location=0) out vec3 texCoordVS;
layout(location=1) out vec2 screenPositionVS;

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

layout(set = 3, binding = 3, std140) uniform UniformBuffer {
    mat4 pMatrix;
    vec4 clipArea;
    vec4 blendArea;
    vec4 characterColor;
    vec4 outlineColor;
    vec2 textOffset;
    float textScale;
    float outlineScale;
    float edgeValue;
    float smoothness;
} uniforms;

void main() {
    
    vec3 characterInfo = instances[gl_InstanceIndex].xyz;

    uint glyphIndex = uint(characterInfo.z);
    
    GlyphInfo glyph = glyphs[glyphIndex];
    
    vec2 position = (vPosition + 1.0) / 2.0;
    screenPositionVS = position * glyph.size * uniforms.textScale + 
        characterInfo.xy * uniforms.textScale + uniforms.textOffset;
    gl_Position = uniforms.pMatrix * vec4(screenPositionVS, 0.0, 1.0);
    texCoordVS = vec3(position * glyph.scale, characterInfo.z);
    
}