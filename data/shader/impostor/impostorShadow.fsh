layout(binding = 0) uniform sampler2DArray baseColorMap;

in vec2 texCoordVS;
flat in int indexVS;

uniform float cutoff;

void main() {

    vec4 baseColor = texture(baseColorMap, vec3(texCoordVS, float(indexVS))).rgba;

    if (baseColor.a < cutoff)
        discard;

}