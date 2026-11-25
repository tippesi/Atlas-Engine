layout(location=0) out vec4 colorFS;

layout(location=0) in vec4 colorVS;
layout(location=1) noperspective in float acrossVS;
layout(location=2) noperspective in float alongVS;
layout(location=3) in float viewDepthVS;

layout(set = 3, binding = 2) uniform sampler2D depthTexture;

layout(push_constant) uniform constants {
    mat4 pMatrix;
    vec2 invResolution;
    vec2 depthParameters;
    float width;
    float occludedAlpha;
    uint testDepth;
} pushConstants;

void main() {

    // Coverage of the pixel by the line
    float alpha = clamp(0.5 * pushConstants.width + 0.5 - abs(acrossVS), 0.0, 1.0);

    // The depth can have another resolution than the target. Lines on a surface, e.g. bounds and the grid, count as
    // in front of it
    float depth = textureLod(depthTexture, gl_FragCoord.xy * pushConstants.invResolution, 0.0).r;
    float sceneViewDepth = pushConstants.depthParameters.y / (depth + pushConstants.depthParameters.x);
    if (pushConstants.testDepth > 0 && viewDepthVS > sceneViewDepth * 1.002 + 0.01) {
        // Dashes tell the hidden parts apart from lines in front of a dark background
        float dash = step(mod(alongVS, 10.0), 6.0);
        alpha *= pushConstants.occludedAlpha * dash;
    }

    if (alpha <= 0.0)
        discard;

    colorFS = vec4(colorVS.rgb, colorVS.a * alpha);

}
