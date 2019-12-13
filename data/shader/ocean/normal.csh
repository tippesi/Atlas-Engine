layout (local_size_x = 16, local_size_y = 16) in;

layout (binding = 0, rgba16f) readonly uniform image2D displacementMap;
layout (binding = 1, rgba8) writeonly uniform image2D normalMap;

uniform int N;
uniform int L;
uniform float choppyScale;
uniform float displacementScale;
uniform float tiling;

vec3 AdjustScale(vec3 point) {

	return vec3(point.x * choppyScale,
		point.y * displacementScale,
		point.z * choppyScale);
		
}

void main() {

	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	
	vec2 fCoord = vec2(coord);
	float fN = float(N);
	
	float texelSize = 2.0 * tiling / fN;
	
	vec3 center = imageLoad(displacementMap, coord).grb;
	vec3 left = imageLoad(displacementMap, ivec2(mod(fCoord.x - 1.0, fN), coord.y)).grb;
	vec3 right = imageLoad(displacementMap, ivec2(mod(fCoord.x + 1.0, fN), coord.y)).grb;
	vec3 top = imageLoad(displacementMap, ivec2(coord.x, mod(fCoord.y + 1.0, fN))).grb;
	vec3 bottom = imageLoad(displacementMap, ivec2(coord.x, mod(fCoord.y - 1.0, fN))).grb;
	
	center = AdjustScale(center);
	left = AdjustScale(left);
	right = AdjustScale(right);
	top = AdjustScale(top);
	bottom = AdjustScale(bottom);

	// Calculate jacobian
	vec2 Dx = (right.xz - left.xz) / texelSize;
	vec2 Dy = (top.xz - bottom.xz) / texelSize;
	float J = (1.0 + Dx.x) * (1.0 + Dy.y) - Dx.y * Dy.x;
	
	float fold = 0.5 * clamp(J, -1.0, 1.0) + 0.5;

	vec3 normal;

	normal.x = left.y - right.y;
	normal.y = texelSize;
	normal.z = bottom.y - top.y;

	normal = 0.5 * normalize(normal) + 0.5;
	
	imageStore(normalMap, coord, vec4(normal, fold));

}