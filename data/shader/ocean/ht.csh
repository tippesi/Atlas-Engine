#include "../common/PI"
#include "../common/complex"

layout (local_size_x = 16, local_size_y = 16) in;

layout (binding = 0, rg32f) writeonly uniform image2D hTDy;
layout (binding = 1, rg32f) writeonly uniform image2D hTDx;
layout (binding = 2, rg32f) writeonly uniform image2D hTDz;

layout (binding = 3, rg32f) readonly uniform image2D h0K;
layout (binding = 4, rg32f) readonly uniform image2D h0MinusK;

uniform int N;
uniform int L;
uniform float time;

const float g = 9.81;

void main() {

	vec2 x = vec2(gl_GlobalInvocationID.xy) - float(N) / 2.0;
	
	vec2 k = vec2(2.0 * PI / float(L)) * x;
	
	float lk = length(k);
	if (lk < 0.0001) lk = 0.0001;
	
	float w = sqrt(g * lk);
	
	complex h0k = complex(imageLoad(h0K, ivec2(gl_GlobalInvocationID.xy)).r,
		imageLoad(h0K, ivec2(gl_GlobalInvocationID.xy)).g);
	complex h0minusk = conj(complex(imageLoad(h0MinusK, ivec2(gl_GlobalInvocationID.xy)).r,
		imageLoad(h0MinusK, ivec2(gl_GlobalInvocationID.xy)).g));
		
	float c = cos(w * time);
	float s = sin(w * time);
	
	complex eiwt = complex(c, s);
	complex eiwtConj = complex(c, -s);
	
	complex hkt_dy = add(mul(h0k, eiwt), mul(h0minusk, eiwtConj));
	
	complex dx = complex(0.0, -k.x / lk);
	complex hkt_dx = mul(dx, hkt_dy);
	
	complex dz = complex(0.0, -k.y / lk);
	complex hkt_dz = mul(dz, hkt_dy);
	
	imageStore(hTDy, ivec2(gl_GlobalInvocationID.xy), vec4(hkt_dy.real, hkt_dy.imag, 0.0, 1.0));
	imageStore(hTDx, ivec2(gl_GlobalInvocationID.xy), vec4(hkt_dx.real, hkt_dx.imag, 0.0, 1.0));
	imageStore(hTDz, ivec2(gl_GlobalInvocationID.xy), vec4(hkt_dz.real, hkt_dz.imag, 0.0, 1.0));
	
}