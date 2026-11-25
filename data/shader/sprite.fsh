layout(location=0) out vec4 colorFS;

layout(location=0) in vec2 texCoordVS;
layout(location=1) in vec4 colorVS;
layout(location=2) flat in vec4 outlineColorVS;
layout(location=3) flat in vec4 uvMinMaxVS;
layout(location=4) flat in float outlineWidthVS;

layout(set = 3, binding = 0) uniform sampler2D spriteTexture;
layout(set = 3, binding = 2) uniform sampler2D depthTexture;

layout(push_constant) uniform constants {
    mat4 pMatrix;
    vec2 invResolution;
    float occludedAlpha;
} pushConstants;

void main() {

    vec4 textureColor = texture(spriteTexture, texCoordVS);
    vec4 color = textureColor * colorVS;

    if (outlineWidthVS > 0.0) {
        // The coverage of the texture grown by the outline width, sampled on two rings
        vec2 texelsPerPixel = fwidth(texCoordVS);
        float outline = 0.0;
        for (int i = 0; i < 16; i++) {
            float angle = float(i) * 0.785398 + (i >= 8 ? 0.392699 : 0.0);
            float radius = i >= 8 ? 0.5 : 1.0;
            vec2 offset = vec2(cos(angle), sin(angle)) * radius * outlineWidthVS * texelsPerPixel;
            vec2 texCoord = clamp(texCoordVS + offset, uvMinMaxVS.xy, uvMinMaxVS.zw);
            outline = max(outline, texture(spriteTexture, texCoord).a);
        }

        color.rgb = mix(outlineColorVS.rgb, color.rgb, textureColor.a);
        color.a = max(color.a, outline * outlineColorVS.a);
    }

    // The depth can have another resolution than the target
    float sceneDepth = textureLod(depthTexture, gl_FragCoord.xy * pushConstants.invResolution, 0.0).r;
    if (gl_FragCoord.z > sceneDepth)
        color.a *= pushConstants.occludedAlpha;

    if (color.a <= 0.0)
        discard;

    colorFS = color;

}
