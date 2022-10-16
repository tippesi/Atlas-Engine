#include <../raytracer/structures.hsh>
#include <../raytracer/common.hsh>
#include <../raytracer/buffers.hsh>
#include <../raytracer/tracing.hsh>
#include <../raytracer/bvh.hsh>
#include <../common/random.hsh>
#include <../common/flatten.hsh>
#include <../common/convert.hsh>
#include <../brdf/importanceSample.hsh>
#include <../common/material.hsh>

layout (local_size_x = 8, local_size_y = 4) in;

layout (binding = 4, rgba16f) writeonly uniform image2D rtrImage;

layout(binding = 16) uniform sampler2D normalTexture;
layout(binding = 17) uniform sampler2D depthTexture;
layout(binding = 18) uniform sampler2D roughnessMetallicAoTexture;
layout(binding = 19) uniform isampler2D offsetTexture;
layout(binding = 20) uniform usampler2D materialIdxTexture;
layout(binding = 21) uniform sampler2D randomTexture;

const ivec2 offsets[4] = ivec2[4](
    ivec2(0, 0),
    ivec2(1, 0),
    ivec2(0, 1),
    ivec2(1, 1)
);

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

        int offsetIdx = texelFetch(offsetTexture, pixel, 0).r;
        ivec2 offset = offsets[offsetIdx];

        float depth = texelFetch(depthTexture, pixel, 0).r;

        vec2 recontructTexCoord = (2.0 * vec2(pixel) + offset + vec2(0.5)) / (2.0 * vec2(resolution));
        vec3 viewPos = ConvertDepthToViewSpace(depth, recontructTexCoord);
	    vec3 worldPos = vec3(ivMatrix * vec4(viewPos, 1.0));
        vec3 viewVec = vec3(ivMatrix * vec4(viewPos, 0.0));
        vec3 worldNorm = normalize(vec3(ivMatrix * vec4(2.0 * textureLod(normalTexture, texCoord, 0).rgb - 1.0, 0.0)));
        vec3 randomVec = normalize(vec3(2.0 * texelFetch(randomTexture, pixel % ivec2(2), 0).xy - 1.0, 0.0));

        uint materialIdx = texelFetch(materialIdxTexture, pixel * 2 + offset, 0).r;
	    Material material = UnpackMaterial(materialIdx);

        float roughness = texelFetch(roughnessMetallicAoTexture, pixel, 0).r;
        material.roughness *= material.roughnessMap ? roughness : 1.0;

        vec3 reflection = vec3(0.0);

        if (material.roughness < 0.3) {

            float raySeed = float(pixel.x + pixel.y * resolution.x);
            float curSeed = float(frameSeed);

            const int sampleCount = 1;
            for (uint i = 0; i < sampleCount; i++) {
                Ray ray;

                float u0 = random(raySeed, curSeed);
                float u1 = random(raySeed, curSeed);

                float pdf;
                ImportanceSampleGGX(vec2(u0, u1), worldNorm, normalize(-viewVec), sqr(material.roughness),
                                    ray.direction, pdf);

                ray.inverseDirection = 1.0 / ray.direction;
                ray.origin = worldPos + ray.direction * EPSILON + worldNorm * EPSILON;

                ray.hitID = -1;
                ray.hitDistance = 0.0;

                HitClosest(ray, 0.0, INF);

                if (ray.hitID >= 0) {
                    Triangle tri = UnpackTriangle(triangles[ray.hitID]);
                    Surface surface = GetSurfaceParameters(tri, ray, true);

                    reflection += min(surface.material.baseColor / pdf, vec3(1.0));
                }
                else {
                    reflection += min(SampleEnvironmentMap(ray.direction).rgb / pdf, vec3(1.0));
                }
            }

            reflection /= float(sampleCount);

        }

        imageStore(rtrImage, pixel, vec4(reflection, 0.0));
	}

}