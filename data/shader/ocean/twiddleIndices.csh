#include "../common/PI"
#include "../common/complex"

layout (local_size_x = 1, local_size_y = 16) in;
layout (binding = 0, rg32f) writeonly uniform image2D twiddleIndicesTexture;

// The first butterfly stage we use bit-reversed indices computed
// on the CPU.
layout (std430, binding = 1) buffer indices {
	int index[];
} bitrevIndices;

uniform int N;

// Remember that this is executed into x direction just log_2(N) times.
void main() {

	vec2 coord = vec2(gl_GlobalInvocationID.xy);
		
	// Span of the butterfly operation at a given horizontal point
	float span = floor(pow(2.0, coord.x));

	bool topWing = mod(coord.y, pow(2.0, coord.x + 1.0)) < pow(2.0, coord.x);
	
	vec4 twiddleIndices;
	
	// For the first stage use the shader storage buffer
	if (coord.x == 0.0) {
		if (topWing)
			twiddleIndices = vec4(bitrevIndices.index[int(coord.y)], 
				bitrevIndices.index[int(coord.y + 1.0)], 0.0, 0.0);
		else
			twiddleIndices = vec4(bitrevIndices.index[int(coord.y - 1.0)],
				bitrevIndices.index[int(coord.y)], 0.0, 0.0);
	}
	else {
		if (topWing)
			twiddleIndices = vec4(coord.y, coord.y + span, 0.0, 0.0);
		else
			twiddleIndices = vec4(coord.y - span, coord.y, 0.0, 0.0);
	}
	
	imageStore(twiddleIndicesTexture, ivec2(coord), twiddleIndices);
	
}