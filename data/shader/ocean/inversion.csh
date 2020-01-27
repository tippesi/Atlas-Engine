layout (local_size_x = 16, local_size_y = 16) in;

layout (binding = 0, rgba16f) writeonly uniform image2D displacementMap;

#ifdef AE_API_GLES
layout (binding = 1, rgba32f) readonly uniform image2D pingpongY0;
#else
layout (binding = 1, rg32f) readonly uniform image2D pingpongY0;
#endif

layout (binding = 3, rgba32f) readonly uniform image2D pingpongXZ0;

uniform int N;
uniform int pingpong;

void main() {

	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	
	float perm = bool((coord.x + coord.y) & 1) ? -1.0 : 1.0;
	
	vec3 displacement;
	
		float x = imageLoad(pingpongXZ0, coord).r;
		float y = imageLoad(pingpongY0, coord).r;
		float z = imageLoad(pingpongXZ0, coord).b;
		
		displacement = perm * vec3(x, y, z); 
	
	imageStore(displacementMap, coord, 
			vec4(displacement, 1.0));


}