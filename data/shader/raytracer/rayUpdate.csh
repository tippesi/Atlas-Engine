#include <structures.hsh>
#include <intersections.hsh>
#include <texture.hsh>
#include <BVH.hsh>
#include <common.hsh>

#include <../common/random.hsh>
#include <../common/utility.hsh>
#include <../common/flatten.hsh>
#include <../common/PI.hsh>

#include <../brdf/brdfEval.hsh>
#include <../brdf/surface.hsh>


//#define GROUP_SHARED_MEMORY

layout (local_size_x = 32) in;

layout (binding = 0, rgba8) writeonly uniform image2D outputImage;

/*
Except for image variables qualified with the format qualifiers r32f, r32i, and r32ui, 
image variables must specify either memory qualifier readonly or the memory qualifier writeonly.
Reading and writing simultaneously to other formats is not supported on OpenGL ES
*/
layout (binding = 1, rgba32f) readonly uniform image2D inAccumImage;
layout (binding = 2, rgba32f) writeonly uniform image2D outAccumImage;

layout (std430, binding = 5) buffer Materials {
	RaytraceMaterial materials[];
};

layout(std430, binding = 0) buffer ReadAtomic {
	uint readAtomic[];
};

layout(std430, binding = 1) buffer WriteAtomic {
	uint writeAtomic[];
};

layout (std430, binding = 3) buffer ReadRays {
	PackedRay readRays[];
};

layout (std430, binding = 4) buffer WriteRays {
	PackedRay writeRays[];
};

// Light
uniform Light light;

uniform int sampleCount;
uniform int bounceCount;

uniform ivec2 resolution;

uniform vec3 cameraLocation;

uniform float seed;

const float gamma = 1.0 / 2.2;

shared uint activeRayMask[1];
shared uint activeRayCount;
shared uint rayOffset;

void EvaluateBounce(inout Ray ray, vec2 coord);
vec3 EvaluateBRDF(Surface surface);

Surface GetSurfaceParameters(Triangle triangle, Ray ray, vec3 intersection);

void main() {
	
	uint index = gl_GlobalInvocationID.x;
	uint localIndex = gl_LocalInvocationID.x;
	
#ifdef GROUP_SHARED_MEMORY
	uint maskOffset = localIndex % uint(32);
	uint maskIndex = localIndex / uint(32);
	
	bool countRays = maskOffset == uint(0) ? true : false;
	
	if (localIndex == uint(0)) {
		activeRayCount = uint(0);
		rayOffset = uint(0);		
	}
	if (countRays) {
		activeRayMask[maskIndex] = uint(0);
	}
	groupMemoryBarrierWithSync();
#endif
	
	bool rayActive = false;
	
	Ray ray;
	
	if (index < readAtomic[0]) {
	
		rayActive = true;
	
		ray = UnpackRay(readRays[index]);
		
		ivec2 pixel = Unflatten2D(ray.pixelID, resolution);
			
		vec2 coord = vec2(pixel) / vec2(resolution);
		
		EvaluateBounce(ray, coord);
		
		vec4 accumColor = vec4(0.0);

		float energy = dot(ray.throughput, vec3(1.0));
		
		if (energy < 1.0 / 1024.0 || bounceCount == 0) {
			if (sampleCount > 0)
				accumColor = imageLoad(inAccumImage, pixel);
			
			// Where does it get negative?
			accumColor += vec4(max(ray.color, vec3(0.0)), 1.0);
			
			imageStore(outAccumImage, pixel, accumColor);

			vec3 color = accumColor.rgb / float(sampleCount + 1);
			color = vec3(1.0) - exp(-color);

			imageStore(outputImage, pixel,
				vec4(pow(color, vec3(gamma)), 1.0));
				
			rayActive = false;
		}
#ifndef GROUP_SHARED_MEMORY
		else {
			uint counter = atomicAdd(writeAtomic[0], uint(1));
			writeRays[counter] = PackRay(ray);
		}
#endif
	}
#ifdef GROUP_SHARED_MEMORY	
	uint maskOr = rayActive ? 1 << maskOffset : 0;

	atomicOr(activeRayMask[maskIndex], maskOr);
	groupMemoryBarrierWithSync();	
	
	if (countRays) {
		atomicAdd(activeRayCount, bitCount(activeRayMask[maskIndex]));
	}
	groupMemoryBarrierWithSync();	
	
	if (localIndex == uint(0)) {
		rayOffset = atomicAdd(writeAtomic[0], activeRayCount);
	}
	groupMemoryBarrierWithSync();
	
	if (rayActive) {
		uint offset = uint(0);
		for (uint i = uint(0); i <= maskIndex; i++) {
			uint mask = 0xFFFFFFFF;
			if (i == maskIndex)
				mask >>= (uint(31) - maskOffset);
			offset += bitCount(activeRayMask[i] & mask);
		}
		uint globalOffset = rayOffset + offset - uint(1);
		writeRays[globalOffset] = PackRay(ray);
	}
	else {
	
	}
#endif

}

void EvaluateBounce(inout Ray ray, vec2 coord) {

	int triangleIndex = 0;
	vec3 intersection;
	vec2 barrycentric = vec2(0.0);
	float curSeed = seed;

	QueryBVHClosest(ray, 0.0, INF, triangleIndex, intersection);
		
	if (intersection.x >= INF) {	
		ray.color += SampleEnvironmentMap(ray.direction).rgb * ray.throughput;
		ray.throughput = vec3(0.0);
		return;	
	}
	
	Triangle tri = UnpackTriangle(triangles[triangleIndex]);
	
	Surface surface = GetSurfaceParameters(tri, ray, intersection);
	Material mat = surface.material;
		
	if (length(mat.emissiveColor) > 0.0001) {	
		ray.color += mat.emissiveColor * ray.throughput;
		ray.throughput = vec3(0.0);
		return;
	}

	ray.color += ray.throughput * EvaluateBRDF(surface);
	ray.origin = surface.P;

	float refractChance = 1.0 - mat.opacity;
	float rnd = random(coord, curSeed);

	float roughness = sqr(mat.roughness);
	vec3 f0 = mix(vec3(0.04), mat.baseColor, mat.metalness);

	if (rnd >= refractChance) {
		rnd = random(coord, curSeed);

		vec3 F = FresnelSchlick(f0, 1.0, saturate(dot(-ray.direction, surface.N)));
		float specChance = dot(F, vec3(0.333));
		
		if (rnd < specChance) {
			vec3 refl = normalize(reflect(ray.direction, surface.N));
			
			float NdotL;
			float pdf;			
			ImportanceSampleCosDir(refl, coord, seed, ray.direction,
				NdotL, pdf);			
			ray.direction = mix(refl, ray.direction, roughness);
			pdf = mix(1.0, pdf, roughness);

			ray.inverseDirection = 1.0 / ray.direction;
			
			ray.throughput = ray.throughput * F / specChance;
			ray.origin += surface.N * EPSILON;
		}
		else {
			float NdotL;
			float pdf;
			ImportanceSampleCosDir(surface.N, coord, seed, ray.direction,
				NdotL, pdf);
			ray.inverseDirection = 1.0 / ray.direction;
				
			ray.origin += surface.N * EPSILON;

			ray.throughput *= mat.baseColor;
			ray.throughput /= (1.0 - specChance);
		}
		
	}
	else {
		ray.throughput *= mix(mat.baseColor, vec3(1.0), refractChance);
	
		ray.origin -= surface.N * EPSILON;
	}

	ray.throughput *= mat.ao;

}

vec3 EvaluateBRDF(Surface surface) {

	surface.L = -light.direction;
	UpdateSurface(surface);
		
	vec3 directDiffuse = EvaluateDiffuseBRDF(surface);
	vec3 directSpecular = EvaluateSpecularBRDF(surface);

	vec3 direct = directSpecular + directDiffuse;
	
	float shadowFactor = 1.0;
	if (surface.NdotL > 0.0) {
		// Shadow testing
		Ray ray;
		ray.direction = surface.L;
		ray.origin = surface.P + surface.N * EPSILON;
		ray.inverseDirection = 1.0 / ray.direction;
		shadowFactor = QueryBVH(ray, 0.0, INF) ? 0.0 : 1.0;
	}
	else {
		shadowFactor = 0.0;
	}

	vec3 radiance = light.color * light.intensity;
	return direct * surface.NdotL * radiance * shadowFactor;

}

Surface GetSurfaceParameters(Triangle tri, Ray ray, vec3 intersection) {

	Surface surface;
	Material mat;

	RaytraceMaterial rayMat = materials[tri.materialIndex];

	mat.baseColor = vec3(rayMat.baseR, rayMat.baseG, rayMat.baseB);
	mat.emissiveColor = vec3(rayMat.emissR, rayMat.emissG, rayMat.emissB);
	mat.transmissiveColor = vec3(0.0);

	mat.opacity = rayMat.opacity;

	mat.roughness = rayMat.roughness;
	mat.metalness = rayMat.metalness;
	mat.ao = rayMat.ao;
	
	mat.normalScale = rayMat.normalScale;
	mat.displacementScale = 0.0;

	float dist = intersection.x;
	vec2 barrycentric = intersection.yz;

	float s = barrycentric.x;
	float t = barrycentric.y;
	float r = 1.0 - s - t;
		
	// Interpolate normal by using barrycentric coordinates
	vec3 position = ray.origin + dist * ray.direction;
	vec2 texCoord = r * tri.uv0 + s * tri.uv1 + t * tri.uv2;
	vec3 normal = r * tri.n0 + s * tri.n1 + t * tri.n2;
		
	texCoord = rayMat.invertUVs > 0 ? vec2(texCoord.x, 1.0 - texCoord.y) : texCoord;
	
	// Produces some problems in the bottom left corner of the Sponza scene,
	// but fixes the cube. Should work in theory.
	normal = dot(normal, ray.direction) <= 0.0 ? normal : normal * -1.0;	
		
	mat.baseColor *= SampleBaseColorBilinear(rayMat.baseColorTexture, texCoord);
	mat.opacity *= SampleOpacityBilinear(rayMat.opacityTexture, texCoord);
	
	mat.roughness *= SampleRoughnessBilinear(rayMat.roughnessTexture, texCoord);
	mat.metalness *= SampleMetalnessBilinear(rayMat.metalnessTexture, texCoord);
	mat.ao *= SampleAoBilinear(rayMat.aoTexture, texCoord);
	
	// Account for changing texture coord direction
	vec3 bitangent = rayMat.invertUVs > 0 ? tri.bt : -tri.bt;
	mat3 TBN = mat3(tri.t, bitangent, normal);
			
	// Sample normal map
	if (rayMat.normalTexture.layer >= 0) {
		vec3 normalColor = 2.0 * SampleNormalBilinear(rayMat.normalTexture, texCoord) - 1.0;
		normal = mix(normal, normalize(TBN * normalColor), mat.normalScale);		
	}

	surface.P = position;
	surface.V = -ray.direction;
	surface.N = normal;
	surface.material = mat;

	return surface;

}