layout(vertices = 16) out;

const int AB = 2;
const int BC = 3;
const int CD = 0;
const int DA = 1;

uniform mat4 vMatrix;
		
void main() {

	if(gl_InvocationID == 0)
	{
	
            vec3 translation = vec3(vMatrix * gl_in[gl_InvocationID].gl_Position);
            
            float distance = length(translation);
            
            int LoD = clamp(int(16.0f - distance), 1, 16);
            
			gl_TessLevelOuter[AB] = LoD;
			gl_TessLevelOuter[BC] = LoD;
			gl_TessLevelOuter[CD] = LoD;
			gl_TessLevelOuter[DA] = LoD;
	
			gl_TessLevelInner[0] = LoD;
			gl_TessLevelInner[1] = LoD;	
	}
	
	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
	
}
