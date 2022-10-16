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

layout (binding = 3, rgba16f) writeonly uniform image2D rtaoImage;

layout(binding = 0) uniform sampler2D normalTexture;
layout(binding = 1) uniform sampler2D depthTexture;
layout(binding = 2) uniform sampler2D roughnessMetallicAoTexture;
layout(binding = 3) uniform isampler2D offsetTexture;
layout(binding = 4) uniform usampler2D materialIdxTexture;
layout(binding = 5) uniform sampler2D randomTexture;

uniform uint sampleCount;
uniform float radius;

uniform mat4 pMatrix;
uniform mat4 ivMatrix;

uniform ivec2 resolution;
uniform float frameSeed;

void main() {

	if (int(gl_GlobalInvocationID.x) < resolution.x &&
		int(gl_GlobalInvocationID.y) < resolution.y) {

		ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
		vec2 texCoord = (vec2(pixel) + vec2(0.5)) / vec2(resolution);

        float depth = texelFetch(depthTexture, pixel, 0).r;

        vec3 viewPos = ConvertDepthToViewSpace(depth, texCoord);
	    vec3 worldPos = vec3(ivMatrix * vec4(viewPos, 1.0));
        vec3 worldNorm = normalize(vec3(ivMatrix * vec4(2.0 * textureLod(normalTexture, texCoord, 0).rgb - 1.0, 0.0)));
        vec3 randomVec = normalize(vec3(2.0 * texelFetch(randomTexture, pixel % ivec2(4), 0).xy - 1.0, 0.0));

        float roughness = texelFetch(roughnessMetallicAoTexture, pixel, 0).r;
        float ao = 0.0;
        
        vec3 up = abs(randomVec.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
        vec3 tangent = normalize(cross(up, randomVec));
        vec3 bitangent = cross(randomVec, tangent);

        mat3 TBN = mat3(tangent, bitangent, randomVec);

        float raySeed = float(randomVec.x);
        float curSeed = float(frameSeed);

        const int sampleCount = 4;
        for (uint i = 0; i < sampleCount; i++) {
            Ray ray;

            float u0 = random(raySeed, curSeed);
    	    float u1 = random(raySeed, curSeed);

            float pdf;
            ImportanceSampleGGX(vec2(u0, u1), worldNorm, -viewPos, roughness,
                                ray.direction, pdf);

            ray.inverseDirection = 1.0 / ray.direction;
            ray.origin = worldPos + ray.direction * 0.1 + worldNorm * 0.1;

            ray.hitID = -1;
            ray.hitDistance = 0.0;

            HitClosest(ray, 0.0, radius);

            ao += ray.hitID > 0 ? saturate(radius - sqr(ray.hitDistance)) : 0.0;
        }

        float result = pow(1.0 - (ao / float(sampleCount)), 1.0);

        imageStore(rtaoImage, pixel, vec4(result, 0.0, 0.0, 0.0));
	}

}