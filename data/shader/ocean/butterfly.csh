#include <../common/PI.hsh>
#include <../common/complex.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

#ifdef AE_API_GLES
layout (binding = 0, rg32f) readonly uniform image2D twiddleIndicesTexture;
#else
layout (binding = 0, rg32f) readonly uniform image2D twiddleIndicesTexture;
#endif

layout (binding = 1, rgba32f) readonly uniform image2D pingpong0;
layout (binding = 2, rgba32f) writeonly uniform image2D pingpong1;

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
	
	float k = mod(float(coord.x) * preTwiddle, float(N));
	float twiddleArgument = 2.0 * PI * k / float(N);
	vec2 twiddleFactor = vec2(cos(twiddleArgument), sin(twiddleArgument));
	
	vec2 twiddle = imageLoad(twiddleIndicesTexture, ivec2(stage, coord.x)).rg;
		
	vec4 s1 = imageLoad(pingpong0, ivec2(twiddle.x, coord.y));
	vec4 t1 = imageLoad(pingpong0, ivec2(twiddle.y, coord.y));
		
	vec4 u1 = vec4(twiddleFactor.x, twiddleFactor.y,
		twiddleFactor.x, twiddleFactor.y);
		
	vec4 H1 = s1 + mul(u1, t1);
		
	imageStore(pingpong1, coord, H1);

}

void vertical() {

	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	
	float k = mod(float(coord.y) * preTwiddle, float(N));
	float twiddleArgument = 2.0 * PI * k / float(N);
	vec2 twiddleFactor = vec2(cos(twiddleArgument), sin(twiddleArgument));
	
	vec2 twiddle = imageLoad(twiddleIndicesTexture, ivec2(stage, coord.y)).rg;
		
	vec4 s1 = imageLoad(pingpong0, ivec2(coord.x, twiddle.x));
	vec4 t1 = imageLoad(pingpong0, ivec2(coord.x, twiddle.y));
		
	vec4 u1 = vec4(twiddleFactor.x, twiddleFactor.y,
		twiddleFactor.x, twiddleFactor.y);
		
	vec4 H1 = s1 + mul(u1, t1);
		
	imageStore(pingpong1, coord, H1);


}