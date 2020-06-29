// See https://eleni.mutantstargoat.com/hikiko/depth-aware-upsampling-2

layout (local_size_x = 8, local_size_y = 8) in;

layout (binding = 0, r32f) writeonly uniform image2D textureOut;
layout (binding = 1) uniform sampler2D textureIn;

float Checkerboard(ivec2 coord) {

    return float((coord.x + coord.y % 2) % 2);

}

float MinDepth(vec4 depth) {

    return min(depth.x, min(depth.y, min(depth.z, depth.w)));

}

float MaxDepth(vec4 depth) {

    return max(depth.x, max(depth.y, max(depth.z, depth.w)));

}

float CheckerboardDepth(vec4 depth, ivec2 coord) {

    return mix(MaxDepth(depth), MinDepth(depth),
        Checkerboard(coord));

}

void main() {

	ivec2 size = textureSize(textureOut, 0);
	ivec2 coord = ivec2(gl_GlobalInvocationID);
	
	if (coord.x < size.x &&
		coord.y < size.y) {

        // Downsample works because coords are range [0,1]
        // and the actual texture size therefore doesn't matter
        vec2 texCoord = vec2(coord) / vec2(size);

		/*
		How gather works:
		r: x0,y1
		g: x1,y1
		b: x1,y0
		a: x0,y0
		Third argument is the component of the source
		To prevent issues: https://stackoverflow.com/questions/46413746/texturegather-behavior-at-texel-center-coordinates
		*/
        vec4 depth = textureGather(textureIn, texCoord, 0);

        float sampleDepth = CheckerboardDepth(depth, coord);
		
		imageStore(textureOut, coord, vec4(sampleDepth, 0.0, 0.0, 1.0));
		
	}

}