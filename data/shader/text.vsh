layout(location=0)in vec2 vPosition;
layout(location=1)in vec3 characterInfo;

out vec3 fTexCoord;
out vec2 fScreenPosition;

uniform mat4 pMatrix;

uniform vec2 textOffset;
uniform float textScale;

struct GlyphInfo {
	vec2 scale;
	vec2 size;
};

layout (std140) uniform UBO1 {
    GlyphInfo glyphs1[1024];
};

layout (std140) uniform UBO2 {
    GlyphInfo glyphs2[1024];
};

void main() {
	
	int glyphIndex = int(characterInfo.z);
	GlyphInfo glyph = glyphIndex > 1023 ? glyphs2[glyphIndex - 1024] : glyphs1[glyphIndex];
	
	vec2 position = (vPosition + 1.0) / 2.0;
	fScreenPosition = position * glyph.size * textScale + characterInfo.xy * textScale + textOffset;
    gl_Position = pMatrix * vec4(fScreenPosition, 0.0, 1.0);
	gl_Position.y *= -1.0f;
    fTexCoord = vec3(position * glyph.scale, characterInfo.z);
	
}