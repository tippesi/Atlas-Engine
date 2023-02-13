layout (location = 0) out vec2 texCoordVS;

void main() {

    // Triangle positions defined inline in shader
    const vec3 positions[6] = vec3[6](
        vec3(1.0, 1.0, 0.0),
        vec3(-1.0, -1.0, 0.0),
        vec3(-1.0, 1.0, 0.0),
        vec3(1.0, -1.0, 0.0),
        vec3(-1.0, -1.0, 0.0),
        vec3(1.0, 1.0, 0.0)
    );

    texCoordVS = positions[gl_VertexIndex].xy * 0.5 + 0.5;

    // Output position of each vertex
    gl_Position = vec4(positions[gl_VertexIndex], 1.0f);

}