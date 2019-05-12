#include "../common/PI"
#include "../common/complex"

layout (local_size_x = 16, local_size_y = 16) in;

layout (binding = 0, rgba32f) uniform image2D twiddleIndicesTexture;

layout (binding = 1, rgba32f) uniform image2D pingpongY0;
layout (binding = 2, rgba32f) uniform image2D pingpongX0;
layout (binding = 3, rgba32f) uniform image2D pingpongZ0;
layout (binding = 4, rgba32f) uniform image2D pingpongY1;
layout (binding = 5, rgba32f) uniform image2D pingpongX1;
layout (binding = 6, rgba32f) uniform image2D pingpongZ1;

uniform int stage;
uniform int pingpong;
uniform int direction;

void horizontal();
void vertical();

void main() {

	if (direction == 0)
		horizontal();
	else
		vertical();

}

void horizontal() {

	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);

	if (pingpong == 0) {
	
		vec4 twiddle = imageLoad(twiddleIndicesTexture, ivec2(stage, coord.x)).rgba;
		
		// Y
		vec2 s = imageLoad(pingpongY0, ivec2(twiddle.z, coord.y)).rg;
		vec2 t = imageLoad(pingpongY0, ivec2(twiddle.w, coord.y)).rg;
		vec2 u = vec2(twiddle.x, twiddle.y);
		
		complex p = complex(s.x, s.y);
		complex q = complex(t.x, t.y);
		complex w = complex(u.x, u.y);
		
		complex H = add(p, mul(w, q));
		
		imageStore(pingpongY1, coord, vec4(H.real, H.imag, 0, 1));
		
		// X
		s = imageLoad(pingpongX0, ivec2(twiddle.z, coord.y)).rg;
		t = imageLoad(pingpongX0, ivec2(twiddle.w, coord.y)).rg;
		u = vec2(twiddle.x, twiddle.y);
		
		p = complex(s.x, s.y);
		q = complex(t.x, t.y);
		w = complex(u.x, u.y);
		
		H = add(p, mul(w, q));
		
		imageStore(pingpongX1, coord, vec4(H.real, H.imag, 0, 1));
		
		// Z
		s = imageLoad(pingpongZ0, ivec2(twiddle.z, coord.y)).rg;
		t = imageLoad(pingpongZ0, ivec2(twiddle.w, coord.y)).rg;
		u = vec2(twiddle.x, twiddle.y);
		
		p = complex(s.x, s.y);
		q = complex(t.x, t.y);
		w = complex(u.x, u.y);
		
		H = add(p, mul(w, q));
		
		imageStore(pingpongZ1, coord, vec4(H.real, H.imag, 0, 1));
	
	}
	else {
	
		vec4 twiddle = imageLoad(twiddleIndicesTexture, ivec2(stage, coord.x)).rgba;
		
		// Y
		vec2 s = imageLoad(pingpongY1, ivec2(twiddle.z, coord.y)).rg;
		vec2 t = imageLoad(pingpongY1, ivec2(twiddle.w, coord.y)).rg;
		vec2 u = vec2(twiddle.x, twiddle.y);
		
		complex p = complex(s.x, s.y);
		complex q = complex(t.x, t.y);
		complex w = complex(u.x, u.y);
		
		complex H = add(p, mul(w, q));
		
		imageStore(pingpongY0, coord, vec4(H.real, H.imag, 0, 1));
		
		// X
		s = imageLoad(pingpongX1, ivec2(twiddle.z, coord.y)).rg;
		t = imageLoad(pingpongX1, ivec2(twiddle.w, coord.y)).rg;
		u = vec2(twiddle.x, twiddle.y);
		
		p = complex(s.x, s.y);
		q = complex(t.x, t.y);
		w = complex(u.x, u.y);
		
		H = add(p, mul(w, q));
		
		imageStore(pingpongX0, coord, vec4(H.real, H.imag, 0, 1));
		
		// Z
		s = imageLoad(pingpongZ1, ivec2(twiddle.z, coord.y)).rg;
		t = imageLoad(pingpongZ1, ivec2(twiddle.w, coord.y)).rg;
		u = vec2(twiddle.x, twiddle.y);
		
		p = complex(s.x, s.y);
		q = complex(t.x, t.y);
		w = complex(u.x, u.y);
		
		H = add(p, mul(w, q));
		
		imageStore(pingpongZ0, coord, vec4(H.real, H.imag, 0, 1));
	
	}

}

void vertical() {

	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	
	if (pingpong == 0) {
	
		vec4 twiddle = imageLoad(twiddleIndicesTexture, ivec2(stage, coord.y)).rgba;
		
		// Y
		vec2 s = imageLoad(pingpongY0, ivec2(coord.x, twiddle.z)).rg;
		vec2 t = imageLoad(pingpongY0, ivec2(coord.x, twiddle.w)).rg;
		vec2 u = vec2(twiddle.x, twiddle.y);
		
		complex p = complex(s.x, s.y);
		complex q = complex(t.x, t.y);
		complex w = complex(u.x, u.y);
		
		complex H = add(p, mul(w, q));
		
		imageStore(pingpongY1, coord, vec4(H.real, H.imag, 0, 1));
		
		// X
		s = imageLoad(pingpongX0, ivec2(coord.x, twiddle.z)).rg;
		t = imageLoad(pingpongX0, ivec2(coord.x, twiddle.w)).rg;
		u = vec2(twiddle.x, twiddle.y);
		
		p = complex(s.x, s.y);
		q = complex(t.x, t.y);
		w = complex(u.x, u.y);
		
		H = add(p, mul(w, q));
		
		imageStore(pingpongX1, coord, vec4(H.real, H.imag, 0, 1));
		
		// Z
		s = imageLoad(pingpongZ0, ivec2(coord.x, twiddle.z)).rg;
		t = imageLoad(pingpongZ0, ivec2(coord.x, twiddle.w)).rg;
		u = vec2(twiddle.x, twiddle.y);
		
		p = complex(s.x, s.y);
		q = complex(t.x, t.y);
		w = complex(u.x, u.y);
		
		H = add(p, mul(w, q));
		
		imageStore(pingpongZ1, coord, vec4(H.real, H.imag, 0, 1));
	
	}
	else {
	
		vec4 twiddle = imageLoad(twiddleIndicesTexture, ivec2(stage, coord.y)).rgba;
		
		// Y
		vec2 s = imageLoad(pingpongY1, ivec2(coord.x, twiddle.z)).rg;
		vec2 t = imageLoad(pingpongY1, ivec2(coord.x, twiddle.w)).rg;
		vec2 u = vec2(twiddle.x, twiddle.y);
		
		complex p = complex(s.x, s.y);
		complex q = complex(t.x, t.y);
		complex w = complex(u.x, u.y);
		
		complex H = add(p, mul(w, q));
		
		imageStore(pingpongY0, coord, vec4(H.real, H.imag, 0, 1));
		
		// X
		s = imageLoad(pingpongX1, ivec2(coord.x, twiddle.z)).rg;
		t = imageLoad(pingpongX1, ivec2(coord.x, twiddle.w)).rg;
		u = vec2(twiddle.x, twiddle.y);
		
		p = complex(s.x, s.y);
		q = complex(t.x, t.y);
		w = complex(u.x, u.y);
		
		H = add(p, mul(w, q));
		
		imageStore(pingpongX0, coord, vec4(H.real, H.imag, 0, 1));
		
		// Z
		s = imageLoad(pingpongZ1, ivec2(coord.x, twiddle.z)).rg;
		t = imageLoad(pingpongZ1, ivec2(coord.x, twiddle.w)).rg;
		u = vec2(twiddle.x, twiddle.y);
		
		p = complex(s.x, s.y);
		q = complex(t.x, t.y);
		w = complex(u.x, u.y);
		
		H = add(p, mul(w, q));
		
		imageStore(pingpongZ0, coord, vec4(H.real, H.imag, 0, 1));
	
	}

}