#include <raytracer/structures.hsh>
#include <raytracer/common.hsh>
#include <raytracer/buffers.hsh>
#include <raytracer/tracing.hsh>
#include <raytracer/bvh.hsh>
#include <common/random.hsh>
#include <common/flatten.hsh>
#include <common/convert.hsh>

layout (local_size_x = 8, local_size_y = 4) in;

layout (binding = 3, r16f) writeonly uniform image2D rtaoImage;

layout(binding = 0) uniform sampler2D normalTexture;
layout(binding = 1) uniform sampler2D shadowMap;
layout(binding = 2) uniform sampler2D randomTexture;

uniform uint sampleCount;
uniform float radius;

uniform mat4 pMatrix;
uniform mat4 ivMatrix;

uniform ivec2 resolution;

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

ivec2 FindNearest2x2(ivec2 pixel, out float depth) {
    float depths[4];
    depths[0] = texelFetch(shadowMap, pixel, 0).r;
    depths[1] = texelFetch(shadowMap, pixel + ivec2(1, 0), 0).r;
    depths[2] = texelFetch(shadowMap, pixel + ivec2(0, 1), 0).r;
    depths[3] = texelFetch(shadowMap, pixel + ivec2(1, 1), 0).r;

    ivec2 depthOffset = ivec2(0, 0);
    depth = depths[0];
    for (uint i = 1; i < 4; i++) {
        if (depths[i] < depth) {
            switch(i) {
                case 1: depthOffset = ivec2(1, 0); break;
                case 2: depthOffset = ivec2(0, 1); break;
                case 3: depthOffset = ivec2(1, 1); break;
            }
            depth = depths[i];
        }
    }

    return depthOffset;
}

void main() {

	if (int(gl_GlobalInvocationID.x) < resolution.x &&
		int(gl_GlobalInvocationID.y) < resolution.y) {

		ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
		vec2 texCoord = (vec2(pixel) + vec2(0.5)) / vec2(resolution);

        float depth;
        ivec2 offset  = FindNearest2x2(pixel * 2, depth);
		
	    vec3 worldPos = vec3(ivMatrix * vec4(ConvertDepthToViewSpace(depth, texCoord), 1.0));
        vec3 worldNorm = normalize(vec3(ivMatrix * vec4(2.0 * texelFetch(normalTexture, pixel * 2 + offset, 0).rgb - 1.0, 0.0)));

        float ao = 0.0;
        
        vec3 up = abs(worldNorm.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
        vec3 tangent = normalize(cross(up, worldNorm));
        vec3 bitangent = cross(worldNorm, tangent);

        mat3 TBN = mat3(tangent, bitangent, worldNorm);

        for (uint i = 0; i < 4; i++) {
            float fibIdx = round(float(12) * random(vec3(texCoord, float(i))));
            Ray ray;
            ray.origin = worldPos + worldNorm * EPSILON;
            ray.direction = normalize(worldNorm + normalize(TBN * fibonacciSphere(fibIdx, float(12))));
            ray.inverseDirection = 1.0 / ray.direction;

            ray.hitID = -1;
            ray.hitDistance = 0.0;

            HitClosest(ray, 0.0, radius);

            ao += ray.hitDistance < radius ? saturate(ray.hitDistance / radius) : 1.0;
        }

        ao /= float(4);

        imageStore(rtaoImage, pixel, vec4(ao, 0.0, 0.0, 0.0));
	}

}