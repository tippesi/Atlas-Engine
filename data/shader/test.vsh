layout (location = 0) out vec2 texCoordVS;

void main()
{
	//const array of positions for the triangle
	const vec3 positions[6] = vec3[6](
		vec3(-1.f,1.f, 0.0f),
		vec3(-1.f,-1.f, 0.0f),
		vec3(1.f,1.f, 0.0f),
		vec3(1.f,1.f, 0.0f),
		vec3(-1.f,-1.f, 0.0f),
		vec3(1.f,-1.f, 0.0f)
	);

	texCoordVS = positions[gl_VertexIndex].xy * 0.5 + 0.5;

	//output the position of each vertex
	gl_Position = vec4(positions[gl_VertexIndex], 1.0f);
}