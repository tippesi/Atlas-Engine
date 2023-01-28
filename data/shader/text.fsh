layout (location = 0) out vec4 colorFS;

layout(location=0) in vec3 texCoordVS;
layout(location=1) in vec2 screenPositionVS;

layout(set = 3, binding = 0) uniform sampler2DArray glyphsTexture;

layout(set = 3, binding = 3, std140) uniform UniformBuffer {
	mat4 pMatrix;
    vec4 clipArea;
    vec4 blendArea;
    vec4 textColor;
    vec4 outlineColor;
    vec2 textOffset;
    float textScale;
    float outlineScale;
    float edgeValue;
    float smoothness;
} uniforms;

void main() {

	colorFS = vec4(0.0);

    if (screenPositionVS.x < uniforms.clipArea.x ||
		screenPositionVS.y < uniforms.clipArea.y ||
        screenPositionVS.x > uniforms.clipArea.x + uniforms.clipArea.z ||
        screenPositionVS.y > uniforms.clipArea.y + uniforms.clipArea.w)
        discard;
	
	float intensity = textureLod(glyphsTexture, texCoordVS, 0).r;
	float smoothing = uniforms.smoothness * fwidth(intensity);
	
	float outlineFactor = smoothstep(uniforms.edgeValue - smoothing, 
		uniforms.edgeValue + smoothing, intensity);
	colorFS = mix(uniforms.outlineColor, uniforms.textColor, outlineFactor);
	
	float mixDistance = mix(uniforms.edgeValue, 0.0, uniforms.outlineScale);
	float alpha = smoothstep(mixDistance - smoothing, mixDistance + smoothing, intensity);
		
	colorFS = uniforms.outlineScale > 0.0 ? vec4(colorFS.rgb, colorFS.a * alpha) : 
		vec4(uniforms.textColor.rgb, uniforms.textColor.a * alpha);

}