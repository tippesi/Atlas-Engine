#include "deferred/convert"

out vec4 fragColor;

in vec2 fTexCoord;
in float depth;
in float blend;

uniform sampler2D diffuseMap;
uniform sampler2D textures[2];
uniform vec2 framebufferResolution;
uniform float brightness;
uniform float fadeFactor;

void main() {
	
	vec2 coord = gl_FragCoord.xy / framebufferResolution;
	
	float sampleDepth = ConvertDepthToViewSpaceDepth(texture(textures[1], coord).r);
	
	float fade = clamp((depth - sampleDepth) * fadeFactor, 0.0, 1.0);
	
	fragColor = texture(diffuseMap, fTexCoord);
	
	fragColor.a *= (blend * fade);
	fragColor.xyz *= brightness;
	
}