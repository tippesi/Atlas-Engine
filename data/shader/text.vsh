layout(location=0)in vec2 vPosition;
layout(location=1)in vec3 characterInfo;

out vec3 fTexCoord;

uniform mat4 pMatrix;

uniform vec2 characterScales[128];
uniform vec2 characterSizes[128];
uniform vec2 characterOffsets[128];

uniform vec2 textOffset;
uniform float textScale;

void main() {
	
	vec2 position = (vPosition + 1.0) / 2.0;
    gl_Position = pMatrix * vec4(position * characterSizes[int(characterInfo.z)] * textScale + characterInfo.xy * textScale + textOffset, 0.0, 1.0);
	gl_Position.y *= -1.0f;
    fTexCoord = vec3(position * characterScales[int(characterInfo.z)], characterInfo.z);
	
}