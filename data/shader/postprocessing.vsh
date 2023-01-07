layout(location=0) out vec2 positionVS;

void main() {

	//const array of positions for the triangle
	const vec3 positions[6] = vec3[6](
		vec3(1.f,1.f, 0.0f),
		vec3(-1.f,-1.f, 0.0f),
		vec3(-1.f,1.f, 0.0f),
		vec3(1.f,-1.f, 0.0f),
		vec3(-1.f,-1.f, 0.0f),
		vec3(1.f,1.f, 0.0f)
		);

	positionVS = positions[gl_VertexIndex].xy;

	//output the position of each vertex
	gl_Position = vec4(positions[gl_VertexIndex], 1.0f);
	
}