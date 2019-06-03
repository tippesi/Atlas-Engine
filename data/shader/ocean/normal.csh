layout (local_size_x = 16, local_size_y = 16) in;

layout (binding = 0, rgba32f) readonly uniform image2D displacementMap;
layout (binding = 1, rgba16f) writeonly uniform image2D normalMap;

uniform int N;
uniform int L;

void main() {

	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	
	vec2 fCoord = vec2(coord);
	float fN = float(N);
	
	float texelSize = 0.01 * float(L) / fN;
	
	vec3 center = imageLoad(displacementMap, coord).rgb;
	vec3 left = imageLoad(displacementMap, ivec2(mod(fCoord.x - 1.0, fN), coord.y)).rgb;
	vec3 right = imageLoad(displacementMap, ivec2(mod(fCoord.x + 1.0, fN), coord.y)).rgb;
	vec3 top = imageLoad(displacementMap, ivec2(coord.x, mod(fCoord.y + 1.0, fN))).rgb;
	vec3 bottom = imageLoad(displacementMap, ivec2(coord.x, mod(fCoord.y - 1.0, fN))).rgb;

	// Calculate jacobian
	vec2 Dx = (right.xz - left.xz);
	vec2 Dy = (top.xz - bottom.xz);
	float J = (1.0 + Dx.x) * (1.0 + Dy.y) - Dx.y * Dy.x;
	
	float fold = clamp(1.0 - J, 0.0, 1.0);
	
	// Calculate difference
	left = left - center + vec3(-texelSize, 0.0, 0.0);
	right = right - center + vec3(texelSize, 0.0, 0.0);
	top = top - center + vec3(0.0, 0.0, -texelSize);
	bottom = bottom - center + vec3(0.0, 0.0, texelSize);
	
	// Calculate normal
	vec3 topLeft = cross(top, left);
	vec3 topRight = cross(right, top);
	vec3 bottomLeft = cross(left, bottom);
	vec3 bottomRight = cross(bottom, right);
	
	vec3 normal = normalize(topLeft + topRight + bottomLeft + bottomRight);
	
	imageStore(normalMap, coord, vec4(normal, fold));

}