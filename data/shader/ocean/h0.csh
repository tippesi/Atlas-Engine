#include "../common/PI"

layout (local_size_x = 16, local_size_y = 16) in;

layout (binding = 0, rgba32f) writeonly uniform image2D h0K;
layout (binding = 1, rgba32f) writeonly uniform image2D h0MinusK;

layout(binding = 2) uniform sampler2D noise0;
layout(binding = 3) uniform sampler2D noise1;
layout(binding = 4) uniform sampler2D noise2;
layout(binding = 5) uniform sampler2D noise3;

uniform int N;
uniform int L;
uniform float A;
uniform vec2 w;
uniform float windspeed;

const float g = 9.81;

// Box-Muller transform: https://en.wikipedia.org/wiki/Box%E2%80%93Muller_transform
vec4 gaussrnd() {

	vec2 coord = vec2(gl_GlobalInvocationID.xy) / float(N);
	
	float U00 = clamp(texture(noise0, coord).r + 0.00001, 0.0, 1.0);
	float U01 = clamp(texture(noise1, coord).r + 0.00001, 0.0, 1.0);
	float U10 = clamp(texture(noise2, coord).r + 0.00001, 0.0, 1.0);
	float U11 = clamp(texture(noise3, coord).r + 0.00001, 0.0, 1.0);
	
	float R0 = sqrt(-2.0 * log(U00));
	float T0 = 2.0 * PI * U01;
	float R1 = sqrt(-2.0 * log(U10));
	float T1 = 2.0 * PI * U11;
	
	return vec4(R0 * cos(T0), R0 * sin(T0),
		R1 * cos(T1), R1 * sin(T1));

}

float phillips(vec2 k, float A, float lkSqd, float l, vec2 w) {

	float kDotw = dot(normalize(k), normalize(w));
	return (A / (lkSqd * lkSqd)) * pow(kDotw * kDotw, 2.0) * exp(-1.0 / (lkSqd * l * l));

}

void main() {

	vec2 x = vec2(gl_GlobalInvocationID.xy) - float(N) / 2.0;
	
	vec2 k = vec2(2.0 * PI / float(L)) * x;
	
	float lk = length(k);	
	if (lk < 0.0001) lk = 0.0001;
	float lkSqd = lk * lk;
	
	float l = (windspeed * windspeed) / g;
	
	float h0k = clamp(sqrt(phillips(k, A, lkSqd, l, w)) / sqrt(2.0), -4000.0, 4000.0);
	float h0minusk = clamp(sqrt(phillips(-k, A, lkSqd, l, w)) / sqrt(2.0), -4000.0, 4000.0);
	
	vec4 rnd = gaussrnd();
	
	imageStore(h0K, ivec2(gl_GlobalInvocationID.xy), vec4(rnd.xy * h0k, 0.0, 1.0));
	imageStore(h0MinusK, ivec2(gl_GlobalInvocationID.xy), vec4(rnd.zw * h0minusk, 0.0, 1.0));

}