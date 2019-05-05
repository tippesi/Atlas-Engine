layout (location = 0) out vec4 color;

in vec3 fTexCoord;
in vec2 fScreenPosition;

layout(binding = 0) uniform sampler2DArray glyphsTexture;
uniform vec4 textColor;
uniform vec4 outlineColor;
uniform float outlineScale;

uniform float edgeValue;
uniform float smoothness;

uniform vec4 clipArea;
uniform vec4 blendArea;

uniform float textScale;

void main() {

	color = vec4(0.0);

    if (fScreenPosition.x < clipArea.x ||
		fScreenPosition.y < clipArea.y ||
        fScreenPosition.x > clipArea.x + clipArea.z ||
        fScreenPosition.y > clipArea.y + clipArea.w)
            discard;
	
	float intensity = texture(glyphsTexture, fTexCoord).r;
	float smoothing = smoothness * fwidth(intensity);
	
	float outlineFactor = smoothstep(edgeValue - smoothing, edgeValue + smoothing, intensity);
	color = mix(outlineColor, textColor, outlineFactor);
	
	float mixDistance = mix(edgeValue, 0.0, outlineScale);
	float alpha = smoothstep(mixDistance - smoothing, mixDistance + smoothing, intensity);
		
	color = outlineScale > 0.0 ? vec4(color.rgb, color.a * alpha) : 
		vec4(textColor.rgb, textColor.a * alpha);

}