// See https://eleni.mutantstargoat.com/hikiko/depth-aware-upsampling-2
// We hijack the step function here to find the min and max indices

layout (local_size_x = 8, local_size_y = 8) in;

layout (binding = 0) uniform sampler2D depthIn;
layout (binding = 1) uniform sampler2D normalIn;
layout (binding = 2) uniform sampler2D geometryNormalIn;
layout (binding = 3) uniform sampler2D roughnessMetallicAoIn;
layout (binding = 4) uniform sampler2D velocityIn;
layout (binding = 5) uniform usampler2D materialIdxIn;

layout (binding = 0) writeonly uniform image2D depthOut;
layout (binding = 1) writeonly uniform image2D normalOut;
layout (binding = 2) writeonly uniform image2D geometryNormalOut;
layout (binding = 3) writeonly uniform image2D roughnessMetallicAoOut;
layout (binding = 4) writeonly uniform image2D velocityOut;
layout (binding = 5) writeonly uniform uimage2D materialIdxOut;
layout (binding = 6) writeonly uniform iimage2D offsetOut;

float Checkerboard(ivec2 coord) {

    return float((coord.x + coord.y % 2) % 2);

}

int MinDepth(vec4 depthVec, out float minDepth) {

	int idx = 0;
#ifndef DEPTH_ONLY    
	minDepth = depthVec[idx];
	idx += int(step(depthVec.y, minDepth));
	minDepth = depthVec[idx];
	idx += int(step(depthVec.z, minDepth));
	minDepth = depthVec[idx];
	idx += int(step(depthVec.w, minDepth));
	minDepth = depthVec[idx];
#else
	minDepth = min(depthVec.x, min(depthVec.y, min(depthVec.z, depthVec.w)));
#endif
	return idx;

}

int MaxDepth(vec4 depthVec, out float maxDepth) {

	int idx = 0;
#ifndef DEPTH_ONLY
	maxDepth = depthVec[idx];
	idx += int(step(maxDepth, depthVec.y));
	maxDepth = depthVec[idx];
	idx += int(step(maxDepth, depthVec.z));
	maxDepth = depthVec[idx];
	idx += int(step(maxDepth, depthVec.w));
	maxDepth = depthVec[idx];
#else
	maxDepth = max(depthVec.x, max(depthVec.y, max(depthVec.z, depthVec.w)));
#endif
	return idx;

}

int CheckerboardDepth(vec4 depthVec, ivec2 coord, out float depth) {

	float minmax = 0.0;

	float maxDepth;
	int maxIdx = MaxDepth(depthVec, maxDepth);
	float minDepth;
	int minIdx = MinDepth(depthVec, minDepth);

    depth =  mix(maxDepth, minDepth, minmax);
	return minmax < 1.0 ? maxIdx : minIdx;

}

void main() {

	ivec2 size = imageSize(depthOut);
	ivec2 coord = ivec2(gl_GlobalInvocationID);
	
	if (coord.x < size.x &&
		coord.y < size.y) {

		float depth00 = texelFetch(depthIn, coord * 2 + ivec2(0, 0), 0).r;
		float depth10 = texelFetch(depthIn, coord * 2 + ivec2(1, 0), 0).r;
		float depth01 = texelFetch(depthIn, coord * 2 + ivec2(0, 1), 0).r;
		float depth11 = texelFetch(depthIn, coord * 2 + ivec2(1, 1), 0).r;

		vec4 depthVec = vec4(depth00, depth10, depth01, depth11);
		float depth = depthVec[0];
        int depthIdx = 0;
		imageStore(depthOut, coord, vec4(depth, 0.0, 0.0, 1.0));

#ifndef DEPTH_ONLY
		vec3 normal00 = texelFetch(normalIn, coord * 2 + ivec2(0, 0), 0).rgb;
		vec3 normal10 = texelFetch(normalIn, coord * 2 + ivec2(1, 0), 0).rgb;
		vec3 normal01 = texelFetch(normalIn, coord * 2 + ivec2(0, 1), 0).rgb;
		vec3 normal11 = texelFetch(normalIn, coord * 2 + ivec2(1, 1), 0).rgb;

		vec3 normal = depthIdx < 2 ? (depthIdx < 1 ? normal00 : normal10) :
			(depthIdx < 3 ? normal01 : normal11);
		imageStore(normalOut, coord, vec4(normal, 1.0));

		vec3 geometryNormal00 = texelFetch(geometryNormalIn, coord * 2 + ivec2(0, 0), 0).rgb;
		vec3 geometryNormal10 = texelFetch(geometryNormalIn, coord * 2 + ivec2(1, 0), 0).rgb;
		vec3 geometryNormal01 = texelFetch(geometryNormalIn, coord * 2 + ivec2(0, 1), 0).rgb;
		vec3 geometryNormal11 = texelFetch(geometryNormalIn, coord * 2 + ivec2(1, 1), 0).rgb;

		vec3 geometryNormal = depthIdx < 2 ? (depthIdx < 1 ? geometryNormal00 : geometryNormal10) :
			(depthIdx < 3 ? geometryNormal01 : geometryNormal11);
		imageStore(geometryNormalOut, coord, vec4(geometryNormal, 1.0));

		vec3 roughnessMetallicAo00 = texelFetch(roughnessMetallicAoIn, coord * 2 + ivec2(0, 0), 0).rgb;
		vec3 roughnessMetallicAo10 = texelFetch(roughnessMetallicAoIn, coord * 2 + ivec2(1, 0), 0).rgb;
		vec3 roughnessMetallicAo01 = texelFetch(roughnessMetallicAoIn, coord * 2 + ivec2(0, 1), 0).rgb;
		vec3 roughnessMetallicAo11 = texelFetch(roughnessMetallicAoIn, coord * 2 + ivec2(1, 1), 0).rgb;

		vec3 roughnessMetallicAo = depthIdx < 2 ? (depthIdx < 1 ? roughnessMetallicAo00 : roughnessMetallicAo10) :
			(depthIdx < 3 ? roughnessMetallicAo01 : roughnessMetallicAo11);
		imageStore(roughnessMetallicAoOut, coord, vec4(roughnessMetallicAo, 1.0));

		vec2 velocity00 = texelFetch(velocityIn, coord * 2 + ivec2(0, 0), 0).rg;
		vec2 velocity10 = texelFetch(velocityIn, coord * 2 + ivec2(1, 0), 0).rg;
		vec2 velocity01 = texelFetch(velocityIn, coord * 2 + ivec2(0, 1), 0).rg;
		vec2 velocity11 = texelFetch(velocityIn, coord * 2 + ivec2(1, 1), 0).rg;

		vec2 velocity = depthIdx < 2 ? (depthIdx < 1 ? velocity00 : velocity10) :
			(depthIdx < 3 ? velocity01 : velocity11);
		imageStore(velocityOut, coord, vec4(velocity, 0.0, 0.0));

		uint materialIdx00 = texelFetch(materialIdxIn, coord * 2 + ivec2(0, 0), 0).r;
		uint materialIdx10 = texelFetch(materialIdxIn, coord * 2 + ivec2(1, 0), 0).r;
		uint materialIdx01 = texelFetch(materialIdxIn, coord * 2 + ivec2(0, 1), 0).r;
		uint materialIdx11 = texelFetch(materialIdxIn, coord * 2 + ivec2(1, 1), 0).r;

		uint materialIdx = depthIdx < 2 ? (depthIdx < 1 ? materialIdx00 : materialIdx10) :
			(depthIdx < 3 ? materialIdx01 : materialIdx11);
		imageStore(materialIdxOut, coord, uvec4(materialIdx, 0u, 0u, 0u));

		imageStore(offsetOut, coord, ivec4(depthIdx, 0, 0, 0));
#endif		
		
	}

}