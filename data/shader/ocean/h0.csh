#include <../common/PI.hsh>

layout (local_size_x = 16, local_size_y = 16) in;

layout (binding = 0, rg32f) writeonly uniform image2D h0K;

layout(binding = 2) uniform sampler2D noise0;
layout(binding = 3) uniform sampler2D noise1;
layout(binding = 4) uniform sampler2D noise2;
layout(binding = 5) uniform sampler2D noise3;

uniform int N;
uniform int L;
uniform float A;
uniform vec2 w;
uniform float windspeed;
uniform float windDependency;
uniform float waveSurpression;

const float g = 9.81;

// Box-Muller transform: https://en.wikipedia.org/wiki/Box%E2%80%93Muller_transform
vec2 gaussrnd() {

	vec2 coord = vec2(gl_GlobalInvocationID.xy) / float(N);
	
	float U00 = clamp(texture(noise0, coord).r + 0.00001, 0.0, 1.0);
	float U01 = clamp(texture(noise1, coord).r + 0.00001, 0.0, 1.0);
	
	float R0 = sqrt(-2.0 * log(U00));
	float T0 = 2.0 * PI * U01;
	
	return vec2(R0 * cos(T0), R0 * sin(T0));

}

float phillips(vec2 k, float A, float l, vec2 w) {

	float k2 = dot(k, k);
	float kCos = dot(k, normalize(w));
	float phillips = A / (k2 * k2 * k2) * (kCos * kCos) * exp(-1.0 / (k2 * l * l));
	
	if (kCos < 0.0)
		phillips *= windDependency;
	
	// Surpress small waves
	l *= waveSurpression;

	return phillips * exp(-k2 * l * l);

}

void main() {

	vec2 coord = vec2(gl_GlobalInvocationID.xy) - float(N) / 2.0;
	
	vec2 waveVector = 2.0 * PI / float(L) * coord;
	
	float l = (windspeed * windspeed) / g;
	
	float h0k = sqrt(phillips(waveVector, A, l, w)) / sqrt(2.0);
	
	vec2 rnd = gaussrnd();
	
	if (waveVector.x == 0.0 &&
		waveVector.y == 0.0) {
		h0k = 0.0;
	}
	
	imageStore(h0K, ivec2(gl_GlobalInvocationID.xy), vec4(rnd.xy * h0k, 0.0, 0.0));
	
}