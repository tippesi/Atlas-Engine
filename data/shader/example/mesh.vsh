layout(location=0) in vec3 vPosition;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec2 vTexCoord;

// Testing macros
#ifdef MYMACRO
layout (location = 0) out vec3 normalVS;
layout (location = 1) out vec2 texCoordVS;
#endif

// Push constant block (note there currently can be only one of these blocks per shader program)
layout(push_constant) uniform constants {
    mat4 vMatrix;
    mat4 pMatrix;
} pushConstants;

// Uniform buffer block
layout(set = 0, binding = 0) uniform  CameraBuffer{
    mat4 vMatrix;
    mat4 pMatrix;
} cameraData;

void main() {

    normalVS = vNormal;
    texCoordVS = vTexCoord;

    //output the position of each vertex
    gl_Position = cameraData.pMatrix * cameraData.vMatrix * vec4(vPosition, 1.0);

}