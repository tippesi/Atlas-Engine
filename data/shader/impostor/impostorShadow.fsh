layout(location = 0) in vec2 texCoordVS;
layout(location = 1) flat in int indexVS;

layout(set = 3, binding = 0) uniform sampler2DArray baseColorMap;

layout(push_constant) uniform constants {
    mat4 lightSpaceMatrix;

    vec4 lightLocation;
    vec4 center;

    float radius;
    int views;
    float cutoff;
} PushConstants;

void main() {

    vec4 baseColor = texture(baseColorMap, vec3(texCoordVS, float(indexVS))).rgba;

    if (baseColor.a < PushConstants.cutoff)
        discard;

}