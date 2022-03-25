layout (local_size_x = 16, local_size_y = 16) in;

layout (binding = 0, rgba16f) writeonly uniform image2D displacementMap;
layout (binding = 1, rgba32f) readonly uniform image2D pingpongMap;

uniform int N;

void main() {

	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	
	float perm = bool((coord.x + coord.y) & 1) ? -1.0 : 1.0;
	
	vec3 displacement;
	
	float x = imageLoad(pingpongMap, coord).b;
	float y = imageLoad(pingpongMap, coord).r;
	float z = imageLoad(pingpongMap, coord).a;
		
	displacement = perm * vec3(x, y, z); 
	
	imageStore(displacementMap, coord, 
			vec4(displacement, 1.0));


}