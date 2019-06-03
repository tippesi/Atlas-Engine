#include "../common/PI"
#include "../common/complex"

layout (local_size_x = 8, local_size_y = 8) in;

layout (binding = 0, rg32f) uniform image2D twiddleIndicesTexture;

layout (binding = 1, rg32f) uniform image2D pingpongY0;
layout (binding = 2, rgba32f) uniform image2D pingpongXZ0;
layout (binding = 3, rg32f) uniform image2D pingpongY1;
layout (binding = 4, rgba32f) uniform image2D pingpongXZ1;

uniform int stage;
uniform int pingpong;
uniform int direction;
uniform int N;
uniform float preTwiddle;

void horizontal();
void vertical();

void main() {

#ifdef HORIZONTAL
	horizontal();
#endif
#ifdef VERTICAL
	vertical();
#endif

}

void horizontal() {

	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	
	float k = mod(coord.x * preTwiddle, float(N));
	float twiddleArgument = 2.0 * PI * k / float(N);
	vec2 twiddleFactor = vec2(cos(twiddleArgument), sin(twiddleArgument));

	if (pingpong == 0) {
	
		vec2 twiddle = imageLoad(twiddleIndicesTexture, ivec2(stage, coord.x)).rg;
		
#ifndef CHOPPY
		// Y
		vec2 s0 = imageLoad(pingpongY0, ivec2(twiddle.x, coord.y)).rg;
		vec2 t0 = imageLoad(pingpongY0, ivec2(twiddle.y, coord.y)).rg;
		
		vec2 H0 = s0 + mul(twiddleFactor, t0);
		
		imageStore(pingpongY1, coord, vec4(H0, 0, 1));
#else
		// XZ
		vec4 s1 = imageLoad(pingpongXZ0, ivec2(twiddle.x, coord.y));
		vec4 t1 = imageLoad(pingpongXZ0, ivec2(twiddle.y, coord.y));
		
		vec4 u1 = vec4(twiddleFactor.x, twiddleFactor.y,
			twiddleFactor.x, twiddleFactor.y);
		
		vec4 H1 = s1 + mul(u1, t1);
		
		imageStore(pingpongXZ1, coord, H1);
#endif
	
	}
	else {
	
		vec2 twiddle = imageLoad(twiddleIndicesTexture, ivec2(stage, coord.x)).rg;
		
#ifndef CHOPPY
		// Y
		vec2 s0 = imageLoad(pingpongY1, ivec2(twiddle.x, coord.y)).rg;
		vec2 t0 = imageLoad(pingpongY1, ivec2(twiddle.y, coord.y)).rg;
		
		vec2 H0 = s0 + mul(twiddleFactor, t0);
		
		imageStore(pingpongY0, coord, vec4(H0, 0, 1));
#else
		// XZ
		vec4 s1 = imageLoad(pingpongXZ1, ivec2(twiddle.x, coord.y));
		vec4 t1 = imageLoad(pingpongXZ1, ivec2(twiddle.y, coord.y));
		
		vec4 u1 = vec4(twiddleFactor.x, twiddleFactor.y,
			twiddleFactor.x, twiddleFactor.y);
		
		vec4 H1 = s1 + mul(u1, t1);
		
		imageStore(pingpongXZ0, coord, H1);
#endif
	
	}

}

void vertical() {

	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	
	float k = mod(coord.y * preTwiddle, float(N));
	float twiddleArgument = 2.0 * PI * k / float(N);
	vec2 twiddleFactor = vec2(cos(twiddleArgument), sin(twiddleArgument));
	
	if (pingpong == 0) {
	
		vec2 twiddle = imageLoad(twiddleIndicesTexture, ivec2(stage, coord.y)).rg;
		
#ifndef CHOPPY
		// Y
		vec2 s0 = imageLoad(pingpongY0, ivec2(coord.x, twiddle.x)).rg;
		vec2 t0 = imageLoad(pingpongY0, ivec2(coord.x, twiddle.y)).rg;
		
		vec2 H0 = s0 + mul(twiddleFactor, t0);
		
		imageStore(pingpongY1, coord, vec4(H0, 0, 1));
#else
		// XZ
		vec4 s1 = imageLoad(pingpongXZ0, ivec2(coord.x, twiddle.x));
		vec4 t1 = imageLoad(pingpongXZ0, ivec2(coord.x, twiddle.y));
		
		vec4 u1 = vec4(twiddleFactor.x, twiddleFactor.y,
			twiddleFactor.x, twiddleFactor.y);
		
		vec4 H1 = s1 + mul(u1, t1);
		
		imageStore(pingpongXZ1, coord, H1);
#endif

	}
	else {
	
		vec2 twiddle = imageLoad(twiddleIndicesTexture, ivec2(stage, coord.y)).rg;
		
#ifndef CHOPPY
		// Y
		vec2 s0 = imageLoad(pingpongY1, ivec2(coord.x, twiddle.x)).rg;
		vec2 t0 = imageLoad(pingpongY1, ivec2(coord.x, twiddle.y)).rg;
		
		vec2 H0 = s0 + mul(twiddleFactor, t0);
		
		imageStore(pingpongY0, coord, vec4(H0, 0, 1));
#else
		// XZ
		vec4 s1 = imageLoad(pingpongXZ1, ivec2(coord.x, twiddle.x));
		vec4 t1 = imageLoad(pingpongXZ1, ivec2(coord.x, twiddle.y));
		
		vec4 u1 = vec4(twiddleFactor.x, twiddleFactor.y,
			twiddleFactor.x, twiddleFactor.y);
		
		vec4 H1 = s1 + mul(u1, t1);
		
		imageStore(pingpongXZ0, coord, H1);
#endif

	}

}