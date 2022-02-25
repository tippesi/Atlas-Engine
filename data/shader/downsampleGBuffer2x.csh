// See https://eleni.mutantstargoat.com/hikiko/depth-aware-upsampling-2
// We hijack the step function here to find the min and max indices

layout (local_size_x = 8, local_size_y = 8) in;

layout (binding = 0) writeonly uniform image2D depthTextureOut;
layout (binding = 1) uniform sampler2D depthTextureIn;

float Checkerboard(ivec2 coord) {

    return float((coord.x + coord.y % 2) % 2);

}

int MinDepth(vec4 depth, out float minDepth) {

    int idx = 0;
	minDepth = depth[idx];
	idx += step(depth.y, minDepth);
	minDepth = depth[idx];
	idx += step(depth.z, minDepth);
	minDepth = depth[idx];
	idx += step(depth.w, minDepth);
	minDepth = depth[idx];
	return idx;

}

int MaxDepth(vec4 depth, out float maxDepth) {

	int idx = 0;
	maxDepth = depth[idx];
	idx += step(maxDepth, depth.y);
	maxDepth = depth[idx];
	idx += step(maxDepth, depth.z);
	maxDepth = depth[idx];
	idx += step(maxDepth, depth.w);
	maxDepth = depth[idx];
	return idx;

}

int CheckerboardDepth(vec4 depth, ivec2 coord, out float depth) {

	float minmax = Checkerboard(coord);

	float maxDepth;
	int maxIdx = MaxDepth(depth, maxDepth);
	float minDepth;
	int minIdx = MinDepth(depth, maxDepth);

    depth =  mix(maxDepth, minDepth, minmax);
	return minmax < 1.0 ? maxIdx : minIdx;

}

void main() {

	ivec2 size = textureSize(textureOut, 0);
	ivec2 coord = ivec2(gl_GlobalInvocationID);
	
	if (coord.x < size.x &&
		coord.y < size.y) {

		float depth00 = texelFetch(depthTextureIn, coord * 2 + ivec2(0, 0), 0);
		float depth10 = texelFetch(depthTextureIn, coord * 2 + ivec2(1, 0), 0);
		float depth01 = texelFetch(depthTextureIn, coord * 2 + ivec2(0, 1), 0);
		float depth11 = texelFetch(depthTextureIn, coord * 2 + ivec2(1, 1), 0);

		vec4 depth = vec4(depth00, depth10, depth01, depth11);
		float downsampledDepth;
        int depthIdx = CheckerboardDepth(depth, coord, downsampledDepth);
		
		imageStore(depthTextureOut, coord, vec4(downsampledDepth, 0.0, 0.0, 1.0));
		
	}

}