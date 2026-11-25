struct Line {
    vec4 from;
    vec4 to;
    uint fromColor;
    uint toColor;
};

layout(set = 3, binding = 1, std430) readonly buffer LineBuffer {
    Line lines[];
};

layout(push_constant) uniform constants {
    mat4 pMatrix;
    vec2 invResolution;
    vec2 depthParameters;
    float width;
    float occludedAlpha;
    uint testDepth;
} pushConstants;

layout(location=0) out vec4 colorVS;
// In pixels: across the line from its center and along it from its start
layout(location=1) noperspective out float acrossVS;
layout(location=2) noperspective out float alongVS;
layout(location=3) out float viewDepthVS;

void main() {

    Line line = lines[gl_InstanceIndex];

    // Triangle strip: the vertices 0 and 2 are at the start, 1 and 3 at the end
    bool atEnd = (gl_VertexIndex & 1) == 1;
    float side = gl_VertexIndex < 2 ? -1.0 : 1.0;

    vec4 fromClip = pushConstants.pMatrix * vec4(line.from.xyz, 1.0);
    vec4 toClip = pushConstants.pMatrix * vec4(line.to.xyz, 1.0);

    vec2 halfResolution = 0.5 / pushConstants.invResolution;
    vec2 fromPixel = fromClip.xy / fromClip.w * halfResolution;
    vec2 toPixel = toClip.xy / toClip.w * halfResolution;

    vec2 delta = toPixel - fromPixel;
    float pixelLength = length(delta);
    vec2 direction = pixelLength > 0.0001 ? delta / pixelLength : vec2(1.0, 0.0);
    vec2 normal = vec2(-direction.y, direction.x);

    // A pixel more than half the width for the anti-aliased edge, the ends are extended as well
    float extent = 0.5 * pushConstants.width + 1.0;
    vec2 offset = normal * side * extent + direction * (atEnd ? extent : -extent);

    vec4 clip = atEnd ? toClip : fromClip;
    clip.xy += offset / halfResolution * clip.w;
    gl_Position = clip;

    colorVS = unpackUnorm4x8(atEnd ? line.toColor : line.fromColor);
    acrossVS = side * extent;
    alongVS = atEnd ? pixelLength + extent : -extent;
    viewDepthVS = -(atEnd ? line.to.z : line.from.z);

}
