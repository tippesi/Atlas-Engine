// Evaluates a part of a graph outside of the regular passes, see materialgraph/Evaluator. Either on a plane for the
// previews of nodes or on a mesh, which is laid out by its texture coordinates to fill maps of it

layout(location=0) out vec3 positionVS;
layout(location=1) out vec3 normalVS;
layout(location=2) out vec2 texCoordVS;
layout(location=3) out vec4 colorVS;
layout(location=4) out vec4 tangentVS;

#ifdef EVALUATE_MESH
layout(location=0) in vec3 vPosition;
layout(location=1) in vec4 vNormal;
layout(location=2) in vec2 vTexCoord;
#ifdef TANGENTS
layout(location=3) in vec4 vTangent;
#endif
#ifdef VERTEX_COLORS
layout(location=4) in vec4 vVertexColors;
#endif
#else
layout(location=0) in vec2 vPosition;
#endif

layout(push_constant) uniform constants {
    float time;
    uint invertUVs;
} PushConstants;

void main() {

    colorVS = vec4(1.0);
    tangentVS = vec4(0.0);

#ifdef EVALUATE_MESH
    positionVS = vPosition;
    normalVS = vNormal.xyz;
    texCoordVS = PushConstants.invertUVs > 0 ? vec2(vTexCoord.x, 1.0 - vTexCoord.y) : vTexCoord;

#ifdef TANGENTS
    tangentVS = vTangent;
#endif
#ifdef VERTEX_COLORS
    colorVS = vVertexColors;
#endif

    // The map is sampled with the coordinates the renderers use, which is what it gets filled with
    gl_Position = vec4(2.0 * texCoordVS - 1.0, 0.0, 1.0);
#else
    // A slightly bulged plane, nodes which depend on the normal show something
    positionVS = vec3(vPosition.x, -vPosition.y, 0.0);
    normalVS = vec3(0.7 * positionVS.xy, 1.0);
    texCoordVS = 0.5 * vPosition + 0.5;
    tangentVS = vec4(1.0, 0.0, 0.0, 1.0);

    gl_Position = vec4(vPosition, 0.0, 1.0);
#endif

}
