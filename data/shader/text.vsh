layout(location=0)in vec2 vPosition;
layout(location=1)in vec3 characterInfo;

out vec3 fTexCoord;
out vec2 fScreenPosition;

uniform mat4 pMatrix;

uniform vec2 characterScales[256];
uniform vec2 characterSizes[256];
uniform vec2 characterOffsets[256];

uniform vec2 textOffset;
uniform float textScale;

void main() {
	
	vec2 position = (vPosition + 1.0) / 2.0;
	fScreenPosition = position * characterSizes[int(characterInfo.z)] * textScale + characterInfo.xy * textScale + textOffset;
    gl_Position = pMatrix * vec4(fScreenPosition, 0.0, 1.0);
	gl_Position.y *= -1.0f;
    fTexCoord = vec3(position * characterScales[int(characterInfo.z)], characterInfo.z);
	
}