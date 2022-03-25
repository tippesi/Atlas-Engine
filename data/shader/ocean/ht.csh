#include <../common/PI.hsh>
#include <../common/complex.hsh>

layout (local_size_x = 16, local_size_y = 16) in;

layout (binding = 0, rgba32f) writeonly uniform image2D hTD;
layout (binding = 1, rg32f) readonly uniform image2D h0K;

uniform int N;
uniform int L;
uniform float time;

const float g = 9.81;

void main() {

	vec2 coord = vec2(gl_GlobalInvocationID.xy) - float(N) / 2.0;
	
	vec2 waveVector = 2.0 * PI / float(L) * coord;
	
	float k = length(waveVector);	
	float w = sqrt(g * k);
	
	ivec2 inverseCoords = (ivec2(imageSize(h0K))) - ivec2(gl_GlobalInvocationID);	
	vec2 h0_k = imageLoad(h0K, ivec2(gl_GlobalInvocationID)).rg;
	vec2 h0_mk = imageLoad(h0K, ivec2(inverseCoords)).rg;
		
	float cos_wt = cos(w * time);
	float sin_wt = sin(w * time);

	// Calculate h(t)
	vec2 ht;
	ht.x = cos_wt * (h0_k.x + h0_mk.x) - sin_wt * (h0_k.y + h0_mk.y);
	ht.y = sin_wt * (h0_k.x - h0_mk.x) + cos_wt * (h0_k.y - h0_mk.y);

	// From h(t) calculate dx(t) and dy(t)
	vec2 kt = coord;
	float kt2 = dot(kt, kt);
	vec2 nkt = vec2(0.0);

	if (kt2 > 1e-12)
		nkt = normalize(kt);

	vec2 dt_x = vec2(ht.y * nkt.x, -ht.x * nkt.x);
	vec2 idt_z = vec2(ht.x * nkt.y, ht.y * nkt.y);
	
	imageStore(hTD, ivec2(gl_GlobalInvocationID.xy), vec4(ht, dt_x + idt_z));
	
}