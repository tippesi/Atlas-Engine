layout(triangles) in;
layout(line_strip, max_vertices = 4) out;

uniform mat4 vMatrix;
uniform mat4 pMatrix;

void main() {
	
	for (int i = 0; i < gl_in.length(); ++i)
	{
		vec4 position = gl_in[i].gl_Position;
		gl_Position = pMatrix * vMatrix * position;
		EmitVertex();
	}
	
	vec4 position = gl_in[0].gl_Position;
#ifdef GEOMETRY_SHADER
	gl_Position = pMatrix * vMatrix * position;
#else
	gl_Position = position;
#endif
    EmitVertex();
	
	EndPrimitive();
}