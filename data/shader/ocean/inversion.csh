layout (local_size_x = 16, local_size_y = 16) in;

layout (binding = 0, rgba32f) writeonly uniform image2D displacementMap;

layout (binding = 1, rgba32f) uniform image2D pingpongY0;
layout (binding = 2, rgba32f) uniform image2D pingpongX0;
layout (binding = 3, rgba32f) uniform image2D pingpongZ0;
layout (binding = 4, rgba32f) uniform image2D pingpongY1;
layout (binding = 5, rgba32f) uniform image2D pingpongX1;
layout (binding = 6, rgba32f) uniform image2D pingpongZ1;

uniform int N;
uniform int pingpong;

void main() {

	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	
	float perm = bool((coord.x + coord.y) & 1) ? -1.0 : 1.0;
	
	if (pingpong == 0) { 
	
		float x = imageLoad(pingpongX0, coord).r;
		float y = imageLoad(pingpongY0, coord).r;
		float z = imageLoad(pingpongZ0, coord).r;
		
		imageStore(displacementMap, coord, 
			vec4(perm * (x / float(N * N)), 
				 perm * (y / float(N * N)),
				 perm * (z / float(N * N)), 1)); 
				 
	} else { 
	
		float x = imageLoad(pingpongX1, coord).r;
		float y = imageLoad(pingpongY1, coord).r;
		float z = imageLoad(pingpongZ1, coord).r;
		
		imageStore(displacementMap, coord, 
			vec4(perm * (x / float(N * N)), 
				 perm * (y / float(N * N)),
				 perm * (z / float(N * N)), 1)); 
	
	}


}