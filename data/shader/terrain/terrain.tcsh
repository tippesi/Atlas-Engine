layout(vertices = 16) out;

const int AB = 2;
const int BC = 3;
const int CD = 0;
const int DA = 1;
		
void main() {

	if(gl_InvocationID == 0)
	{
			gl_TessLevelOuter[AB] = 1;
			gl_TessLevelOuter[BC] = 1;
			gl_TessLevelOuter[CD] = 1;
			gl_TessLevelOuter[DA] = 1;
	
			gl_TessLevelInner[0] = 1;
			gl_TessLevelInner[1] = 1;	
	}
	
	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
	
}