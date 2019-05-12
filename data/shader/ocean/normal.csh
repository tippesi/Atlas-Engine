layout (local_size_x = 16, local_size_y = 16) in;

layout (binding = 0, rgba32f) readonly uniform image2D displacementMap;
layout (binding = 1, rgba16f) writeonly uniform image2D normalMap;

uniform int N;

void main() {

	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	
	vec3 dispL = imageLoad(displacementMap, ivec2((coord.x - 1) % N, coord.y)).rgb;
	vec3 dispR = imageLoad(displacementMap, ivec2((coord.x + 1) % N, coord.y)).rgb;
	vec3 dispT = imageLoad(displacementMap, ivec2(coord.x, (coord.y + 1) % N)).rgb;
	vec3 dispD = imageLoad(displacementMap, ivec2(coord.x, (coord.y - 1) % N)).rgb;
	
	float y = 3.2;
	float x = (dispL.y - dispR.y);
	float z = dispD.y - dispT.y;
	
	vec3 normal = normalize(vec3(x, y, z));
	
	// Jacobian
	vec2 Dx = (dispR.xz - dispL.xz) * .75;
	vec2 Dy = (dispT.xz - dispD.xz) * .75;
	float J = (1.0 + Dx.x) * (1.0 + Dy.y) - Dx.y * Dy.x;
	
	float fold = clamp(1.0 - J, 0.0, 1.0);
	
	imageStore(normalMap, coord, vec4(normal, fold));

}