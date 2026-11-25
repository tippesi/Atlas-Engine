struct Sprite {
    // The w component is 1 for a size in screen space
    vec4 viewPosition;
    vec4 uvMinMax;
    vec2 size;
    uint color;
    uint outlineColor;
    float outlineWidth;
};

layout(set = 3, binding = 1, std430) readonly buffer SpriteBuffer {
    Sprite sprites[];
};

layout(push_constant) uniform constants {
    mat4 pMatrix;
    vec2 invResolution;
    float occludedAlpha;
} pushConstants;

layout(location=0) out vec2 texCoordVS;
layout(location=1) out vec4 colorVS;
layout(location=2) flat out vec4 outlineColorVS;
layout(location=3) flat out vec4 uvMinMaxVS;
layout(location=4) flat out float outlineWidthVS;

void main() {

    Sprite sprite = sprites[gl_InstanceIndex];

    // Triangle strip from the top left corner of the screen
    vec2 corner = vec2(gl_VertexIndex & 1, gl_VertexIndex >> 1);
    vec2 offset = corner * 2.0 - 1.0;

    bool screenSpaceSize = sprite.viewPosition.w > 0.0;

    vec3 viewPosition = sprite.viewPosition.xyz;
    if (!screenSpaceSize)
        viewPosition.xy += vec2(offset.x, -offset.y) * 0.5 * sprite.size;

    gl_Position = pushConstants.pMatrix * vec4(viewPosition, 1.0);
    if (screenSpaceSize)
        gl_Position.xy += offset * sprite.size * pushConstants.invResolution * gl_Position.w;

    texCoordVS = mix(sprite.uvMinMax.xy, sprite.uvMinMax.zw, corner);
    colorVS = unpackUnorm4x8(sprite.color);
    outlineColorVS = unpackUnorm4x8(sprite.outlineColor);
    uvMinMaxVS = sprite.uvMinMax;
    outlineWidthVS = sprite.outlineWidth;

}
