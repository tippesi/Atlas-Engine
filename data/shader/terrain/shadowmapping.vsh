layout (location = 0) in vec3 vPosition;

layout (set = 3, binding = 0) uniform usampler2D heightField;

out float gl_ClipDistance[1];

layout(push_constant) uniform constants {
    mat4 lightSpaceMatrix;
    vec4 translation;

    float nodeSideLength;
    float tileScale;
    float patchSize;
    float heightScale;

    float leftLoD;
    float topLoD;
    float rightLoD;
    float bottomLoD;

    vec2 nodeLocation;
} PushConstants;

vec2 stitch(vec2 position) {

    // We have 8x8 patches per node
    float nodeSize = 8.0 * PushConstants.patchSize;

    if (position.x == 0.0 && PushConstants.leftLoD > 1.0) {
        position.y = floor(position.y / PushConstants.leftLoD)
        * PushConstants.leftLoD;
    }
    else if (position.y == 0.0 && PushConstants.topLoD > 1.0) {
        position.x = floor(position.x / PushConstants.topLoD)
        * PushConstants.topLoD;
    }
    else if (position.x == nodeSize && PushConstants.rightLoD > 1.0) {
        position.y = floor(position.y / PushConstants.rightLoD)
        * PushConstants.rightLoD;
    }
    else if (position.y == nodeSize && PushConstants.bottomLoD > 1.0) {
        position.x = floor(position.x / PushConstants.bottomLoD)
        * PushConstants.bottomLoD;
    }

    return position;

}

void main() {

    vec2 localPosition = vPosition.xz;

    localPosition = stitch(localPosition) * PushConstants.tileScale;

    vec2 position = PushConstants.nodeLocation + localPosition;

    vec2 texCoords = localPosition;
    texCoords /= PushConstants.nodeSideLength;

    uint heightDiscretized = texture(heightField, texCoords).r;
    // The middle of the texel should match the vertex position, 65535 marks holes
    float height = float(heightDiscretized) / 65534.0 * PushConstants.heightScale;

    gl_Position =  PushConstants.lightSpaceMatrix * vec4(vec3(position.x, height, position.y) + PushConstants.translation.xyz, 1.0);

    // The triangles around holes are clipped, see terrain.vsh
    gl_ClipDistance[0] = heightDiscretized == 65535u ? -1e20 : 1.0;

}