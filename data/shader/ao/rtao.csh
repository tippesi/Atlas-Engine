#include <../raytracer/structures.hsh>
#include <../raytracer/common.hsh>
#include <../raytracer/buffers.hsh>
#include <../raytracer/tracing.hsh>
#include <../raytracer/bvh.hsh>
#include <../common/random.hsh>
#include <../common/flatten.hsh>
#include <../common/convert.hsh>
#include <../brdf/brdfSample.hsh>

layout (local_size_x = 8, local_size_y = 4) in;

layout (binding = 3, r16f) writeonly uniform image2D rtaoImage;

layout(binding = 0) uniform sampler2D normalTexture;
layout(binding = 1) uniform sampler2D shadowMap;
layout(binding = 2) uniform sampler2D randomTexture;
layout(binding = 3) uniform isampler2D offsetTexture;

const ivec2 offsets[4] = ivec2[4](
    ivec2(0, 0),
    ivec2(1, 0),
    ivec2(0, 1),
    ivec2(1, 1)
);

uniform int sampleCount;
uniform float radius;

uniform mat4 pMatrix;
uniform mat4 ivMatrix;

uniform ivec2 resolution;
uniform uint frameSeed;

void main() {

    int verticalGroupCount = resolution.y / int(gl_WorkGroupSize.y);
    verticalGroupCount += ((verticalGroupCount * int(gl_WorkGroupSize.y) == resolution.y) ? 0 : 1);

    int tileSize = 4;
    int groupsPerTile = tileSize * verticalGroupCount;

    int tileGroupIdx = int(gl_WorkGroupID.x) % groupsPerTile;
    int tileIdx = int(gl_WorkGroupID.x) / groupsPerTile;

    ivec2 tileBaseOffset = ivec2(tileIdx * tileSize, 0);
    ivec2 tileGroupOffset = ivec2(tileGroupIdx % tileSize, tileGroupIdx / tileSize);

    ivec2 groupOffset = (tileBaseOffset + tileGroupOffset) * ivec2(gl_WorkGroupSize);
    ivec2 pixel = ivec2(gl_LocalInvocationID.xy) + groupOffset;

	if (int(pixel.x) < resolution.x &&
		int(pixel.y) < resolution.y) {

		vec2 texCoord = (vec2(pixel) + vec2(0.5)) / vec2(resolution);

        float depth = texelFetch(shadowMap, pixel, 0).r;
		
        int offsetIdx = texelFetch(offsetTexture, pixel, 0).r;
        ivec2 offset = offsets[offsetIdx];

        vec2 recontructTexCoord = (2.0 * vec2(pixel) + offset + vec2(0.5)) / (2.0 * vec2(resolution));
	    vec3 worldPos = vec3(ivMatrix * vec4(ConvertDepthToViewSpace(depth, recontructTexCoord), 1.0));
        vec3 worldNorm = normalize(vec3(ivMatrix * vec4(2.0 * textureLod(normalTexture, texCoord, 0).rgb - 1.0, 0.0)));
        float seed = texelFetch(randomTexture, pixel % ivec2(4), 0).r;

        float ao = 0.0;

        float raySeed = float(seed);
        float curSeed = float(0);

        ivec2 noiseOffset = Unflatten2D(int(frameSeed), ivec2(16)) * ivec2(8);
        vec2 blueNoiseVec = texelFetch(randomTexture, ((pixel % ivec2(8)) + noiseOffset) % ivec2(128), 0).xy * 256.0;
		blueNoiseVec = clamp(blueNoiseVec, 0.0, 255.0);
		blueNoiseVec = (blueNoiseVec + 0.5) / 256.0;

        const int sampleCount = 1;
        for (uint i = 0; i < sampleCount; i++) {
            Ray ray;
            Surface surface;

            float u0 = random(raySeed, curSeed);
    	    float u1 = random(raySeed, curSeed);

            surface.N = worldNorm;
            surface.P = worldPos;
            BRDFSample brdfSample = SampleDiffuseBRDF(surface, blueNoiseVec);
           
            ray.direction = brdfSample.L;
            ray.inverseDirection = 1.0 / ray.direction;
            ray.origin = worldPos + ray.direction * EPSILON + worldNorm * EPSILON;

            ray.hitID = -1;
            ray.hitDistance = 0.0;

            bool hit = HitAny(ray, 0.0, radius);

            ao += hit ? 1.0 : 0.0;
        }

        float result = 1.0 - (ao / float(sampleCount));

        imageStore(rtaoImage, pixel, vec4(result, 0.0, 0.0, 0.0));
	}

}