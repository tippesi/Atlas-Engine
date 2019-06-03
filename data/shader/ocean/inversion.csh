layout (local_size_x = 16, local_size_y = 16) in;

layout (binding = 0, rgba32f) writeonly uniform image2D displacementMap;

layout (binding = 1, rg32f) uniform image2D pingpongY0;
layout (binding = 2, rgba32f) uniform image2D pingpongXZ0;
layout (binding = 3, rg32f) uniform image2D pingpongY1;
layout (binding = 4, rgba32f) uniform image2D pingpongXZ1;

uniform int N;
uniform int pingpong;

void main() {

	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	
	float perm = bool((coord.x + coord.y) & 1) ? -1.0 : 1.0;
	
	vec3 displacement;
	
	if (pingpong == 0) { 
	
		float x = imageLoad(pingpongXZ0, coord).r;
		float y = imageLoad(pingpongY0, coord).r;
		float z = imageLoad(pingpongXZ0, coord).b;
		
		displacement = perm * vec3(x, y, z); 
				 
	} else { 
	
		float x = imageLoad(pingpongXZ1, coord).r;
		float y = imageLoad(pingpongY1, coord).r;
		float z = imageLoad(pingpongXZ1, coord).b;
		
		displacement = perm * vec3(x, y, z);
	
	}
	
	imageStore(displacementMap, coord, 
			vec4(displacement, 1.0));


}