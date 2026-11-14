// Evaluates a part of a graph outside of the regular passes, see materialgraph/Evaluator. Either on a plane for the
// previews of nodes, on a mesh, which is laid out by its texture coordinates to fill maps of it, or on a mesh seen
// from the views of an impostor

layout(location=0) out vec3 positionVS;
layout(location=1) out vec3 normalVS;
layout(location=2) out vec2 texCoordVS;
layout(location=3) out vec4 colorVS;
layout(location=4) out vec4 tangentVS;
#ifdef EVALUATE_IMPOSTOR
layout(location=5) out float depthVS;
#endif

#ifdef EVALUATE_MESH
layout(location=0) in vec3 vPosition;
layout(location=1) in vec4 vNormal;
#ifdef TEX_COORDS
layout(location=2) in vec2 vTexCoord;
#endif
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
#ifdef EVALUATE_IMPOSTOR
    mat4 vMatrix;
#endif
    float time;
    uint invertUVs;
#ifdef EVALUATE_IMPOSTOR
    float distanceToPlaneCenter;
    uint twoSided;
#endif
} PushConstants;

#ifdef EVALUATE_IMPOSTOR
layout(set = 3, binding = 7, std140) uniform UniformBuffer {
    mat4 pMatrix;
} Uniforms;
#endif

void main() {

    colorVS = vec4(1.0);
    tangentVS = vec4(0.0);

#ifdef EVALUATE_MESH
    positionVS = vPosition;
    normalVS = vNormal.xyz;
    texCoordVS = vec2(0.0);
#ifdef TEX_COORDS
    texCoordVS = PushConstants.invertUVs > 0 ? vec2(vTexCoord.x, 1.0 - vTexCoord.y) : vTexCoord;
#endif

#ifdef TANGENTS
    tangentVS = vTangent;
#endif
#ifdef VERTEX_COLORS
    colorVS = vVertexColors;
#endif

#ifdef EVALUATE_IMPOSTOR
    vec4 positionToCamera = PushConstants.vMatrix * vec4(vPosition, 1.0);
    depthVS = positionToCamera.z + PushConstants.distanceToPlaneCenter;
    gl_Position = Uniforms.pMatrix * positionToCamera;
#else
    // The map is sampled with the coordinates the renderers use, which is what it gets filled with
    gl_Position = vec4(2.0 * texCoordVS - 1.0, 0.0, 1.0);
#endif
#else
    // A slightly bulged plane, nodes which depend on the normal show something
    positionVS = vec3(vPosition.x, -vPosition.y, 0.0);
    normalVS = vec3(0.7 * positionVS.xy, 1.0);
    texCoordVS = 0.5 * vPosition + 0.5;
    tangentVS = vec4(1.0, 0.0, 0.0, 1.0);

    gl_Position = vec4(vPosition, 0.0, 1.0);
#endif

}
