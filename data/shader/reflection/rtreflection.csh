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
layout(binding = 1) uniform sampler2D depthTexture;
layout(binding = 2) uniform sampler2D roughnessMetallicAoTexture;
layout(binding = 3) uniform sampler2D randomTexture;

uniform uint sampleCount;
uniform float radius;

uniform mat4 pMatrix;
uniform mat4 ivMatrix;

uniform ivec2 resolution;
uniform float frameSeed;

/*
Spherical Fibonacci Mapping
http://lgdv.cs.fau.de/publications/publication/Pub.2015.tech.IMMD.IMMD9.spheri/
Authors: Benjamin Keinert, Matthias Innmann, Michael Sänger, Marc Stamminger
*/
float madfrac(float a, float b) { return a * b - floor(a * b); }

vec3 fibonacciSphere(float i, float n) {
	const float PHI = 1.6180339887498948482045868343656;
	float phi = 2.0 * PI * madfrac(i, PHI);
	float cosTheta = 1.0 - (2.0 * i + 1.0) * (1.0 / n);
	float sinTheta = sqrt(saturate(1.0 - sqr(cosTheta)));
	return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

void main() {

	if (int(gl_GlobalInvocationID.x) < resolution.x &&
		int(gl_GlobalInvocationID.y) < resolution.y) {

		ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
		vec2 texCoord = (vec2(pixel) + vec2(0.5)) / vec2(resolution);

        float depth = texelFetch(depthTexture, pixel, 0).r;
		
	    vec3 worldPos = vec3(ivMatrix * vec4(ConvertDepthToViewSpace(depth, texCoord), 1.0));
        vec3 worldNorm = normalize(vec3(ivMatrix * vec4(2.0 * textureLod(normalTexture, texCoord, 0).rgb - 1.0, 0.0)));
        vec3 randomVec = normalize(vec3(2.0 * texelFetch(randomTexture, pixel % ivec2(4), 0).xy - 1.0, 0.0));

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
            Surface surface;

            float u0 = random(raySeed, curSeed);
    	    float u1 = random(raySeed, curSeed);

            surface.N = worldNorm;
            surface.P = worldPos;
            BRDFSample brdfSample = SampleDiffuseBRDF(surface, vec2(u0, u1));
           
            ray.direction = brdfSample.L;
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