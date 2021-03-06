#include <../common/PI.hsh>
#include <../common/complex.hsh>

layout (local_size_x = 16, local_size_y = 16) in;

#ifdef AE_API_GLES
layout (binding = 0, rgba32f) writeonly uniform image2D hTDy;
#else
layout (binding = 0, rg32f) writeonly uniform image2D hTDy;
#endif

layout (binding = 1, rgba32f) writeonly uniform image2D hTDxz;

layout (binding = 2, rgba32f) readonly uniform image2D h0K;

uniform int N;
uniform int L;
uniform float time;

const float g = 9.81;

void main() {

	vec2 coord = vec2(gl_GlobalInvocationID.xy) - float(N) / 2.0;
	
	vec2 waveVector = 2.0 * PI / float(L) * coord;
	
	float k = length(waveVector);
	if (k < 0.0001) k = 0.0001;
	
	float w = sqrt(g * k);
	
	Complex h0k = Complex(imageLoad(h0K, ivec2(gl_GlobalInvocationID.xy)).r,
		imageLoad(h0K, ivec2(gl_GlobalInvocationID.xy)).g);
	Complex h0mk = conj(Complex(imageLoad(h0K, ivec2(gl_GlobalInvocationID.xy)).b,
		imageLoad(h0K, ivec2(gl_GlobalInvocationID.xy)).a));
		
	float c = cos(w * time);
	float s = sin(w * time);
	
	Complex eiwt = Complex(c, s);
	Complex eiwtConj = Complex(c, -s);
	
	Complex hkt_dy = add(mul(h0k, eiwt), mul(h0mk, eiwtConj));
	
	Complex dx = Complex(0.0, -waveVector.x / k);
	Complex hkt_dx = mul(dx, hkt_dy);
	
	Complex dz = Complex(0.0, -waveVector.y / k);
	Complex hkt_dz = mul(dz, hkt_dy);
	
	if (waveVector.x == 0.0 &&
		waveVector.y == 0.0) {
		hkt_dy = Complex(0.0, 0.0);
		hkt_dx = Complex(0.0, 0.0);
		hkt_dz = Complex(0.0, 0.0);
	}
	
	imageStore(hTDy, ivec2(gl_GlobalInvocationID.xy), 
		vec4(hkt_dy.real, hkt_dy.imag, 0.0, 1.0));
	imageStore(hTDxz, ivec2(gl_GlobalInvocationID.xy),
		vec4(hkt_dx.real, hkt_dx.imag, hkt_dz.real, hkt_dz.imag));
	
}