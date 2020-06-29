#include <structures.hsh>
#include <intersections.hsh>
#include <texture.hsh>
#include <../common/random.hsh>
#include <BVH.hsh>
#include <common.hsh>
#include <../brdf/brdf.hsh>
#include <../common/utility.hsh>

#include <../common/PI.hsh>

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
	Material materials[];
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

shared uint activeRayMask[4];
shared uint activeRayCount;
shared uint rayOffset;

void Radiance(inout Ray ray, vec2 coord);
void DirectIllumination(Ray ray, vec3 position, vec3 normal, Material material, 
	vec3 baseColor, out vec3 color);

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
		
		ivec2 pixel = ivec2(ray.pixelID % resolution.x,
			ray.pixelID / resolution.x);
			
		vec2 coord = vec2(pixel) / vec2(resolution);
		
		Radiance(ray, coord);
		
		vec4 accumColor = vec4(0.0);

		float energy = ray.throughput.r + ray.throughput.g
			 + ray.throughput.b;
		
		if (energy < 1.0 / 1024.0 || bounceCount == 0) {
			if (sampleCount > 0)
				accumColor = imageLoad(inAccumImage, pixel);
			
			// Where does it get negative?
			accumColor += vec4(max(ray.color, vec3(0.0)), 1.0);
			
			// Maybe we should substract the jitter to get a sharper image
			imageStore(outAccumImage, pixel, accumColor);

			vec4 color = accumColor / float(sampleCount + 1);
			color = vec4(1.0) - exp(-color);

			imageStore(outputImage, pixel,
				pow(color, vec4(gamma)));
				
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

void Radiance(inout Ray ray, vec2 coord) {

	int triangleIndex = 0;
	vec3 intersection;
	vec2 barrycentric = vec2(0.0);
	float curSeed = seed;

	QueryBVHClosest(ray, 0.0, INF, triangleIndex, intersection);
	barrycentric = intersection.yz;
		
	if (intersection.x >= INF) {	
		ray.color += SampleEnvironmentMap(ray.direction).rgb * ray.throughput;
		ray.throughput = vec3(0.0);
		return;	
	}
		
	Triangle tri = UnpackTriangle(triangles[triangleIndex]);
	Material mat = materials[tri.materialIndex];
		
	vec3 emissiveColor = vec3(mat.emissR, mat.emissG, mat.emissB);
		
	if (length(emissiveColor) > 0.0001) {	
		ray.color += emissiveColor * ray.throughput;
		ray.throughput = vec3(0.0);
		return;
	}		
		
	float s = barrycentric.x;
	float t = barrycentric.y;
	float r = 1.0 - s - t;
		
	// Interpolate normal by using barrycentric coordinates
	vec3 position = ray.origin + intersection.x * ray.direction;
	vec2 texCoord = r * tri.uv0 + s * tri.uv1 + t * tri.uv2;
	vec3 normal = r * tri.n0 + s * tri.n1 + t * tri.n2;
		
	texCoord = mat.invertUVs > 0 ? vec2(texCoord.x, 1.0 - texCoord.y) : texCoord;
	
	// Produces some problems in the bottom left corner of the Sponza scene,
	// but fixes the cube. Should work in theory.
	normal = dot(normal, ray.direction) <= 0.0 ? normal : normal * -1.0;	
		
	vec4 textureColor = SampleBaseColorBilinear(mat.baseColorTexture, texCoord);
	vec3 baseColor = vec3(mat.baseR, mat.baseG, mat.baseB) * textureColor.rgb;
	
	// Account for changing texture coord direction
	vec3 bitangent = mat.invertUVs > 0 ? tri.bt : -tri.bt;
	mat3 toTangentSpace = mat3(tri.t, bitangent, normal);		
			
	// Sample normal map
	if (mat.normalTexture.layer >= 0) {
		normal = mix(normal, normalize(toTangentSpace * 
			(2.0 * vec3(SampleNormalBilinear(mat.normalTexture, texCoord)) - 1.0)),
			mat.normalScale);		
	}
	
	mat.roughness *= SampleRoughnessBilinear(mat.roughnessTexture, texCoord).r;
	mat.metalness *= SampleMetalnessBilinear(mat.metalnessTexture, texCoord).r;
	mat.ao *= SampleAoBilinear(mat.aoTexture, texCoord).r;
		
	vec3 direct = vec3(0.0);
	DirectIllumination(ray, position, normal, mat, baseColor, direct);	
		
	ray.origin = position;
	
	float opacity = textureColor.a * mat.opacity;
	float refractChance = 1.0 - opacity;
	float rnd = random(coord, curSeed);

	float roughness = sqr(mat.roughness);
	vec3 f0 = mix(vec3(0.04), baseColor, mat.metalness);

	if (rnd >= refractChance) {
		rnd = random(coord, curSeed);

		vec3 F = FresnelSchlick(f0, 1.0, saturate(dot(-ray.direction, normal)));
		float specChance = dot(F, vec3(0.333));
		specChance = 0.0;
		
		if (rnd < specChance) {
			vec3 refl = normalize(reflect(ray.direction, normal));
			
			float NdotL;
			float pdf;
			ImportanceSampleCosDir(refl, coord, seed, ray.direction,
				NdotL, pdf);			
			ray.direction = mix(refl, ray.direction, roughness);
			pdf = mix(1.0, pdf, roughness);
			ray.inverseDirection = 1.0 / ray.direction;
			
			ray.throughput *= F / (specChance);
			ray.origin += normal * EPSILON;
		}
		else {
			float NdotL;
			float pdf;
			ImportanceSampleCosDir(normal, coord, seed, ray.direction,
				NdotL, pdf);
			ray.inverseDirection = 1.0 / ray.direction;
				
			ray.origin += normal * EPSILON;			
				
			ray.throughput *= baseColor;
			ray.throughput /= (1.0 - specChance);
		}
		
	}
	else {
		float ior = 2.0 / (1.0 - sqrt(max(f0.x, max(f0.y, f0.z)))) - 1.0;
		vec3 refr = normalize(refract(ray.direction, normal, 1.0 - ior));
		
		ray.direction = HemisphereCos(refr, coord, seed);
		ray.direction = mix(refr, ray.direction, roughness);
		ray.inverseDirection = 1.0 / ray.direction;

		ray.throughput *= mix(baseColor, vec3(1.0), refractChance);
		ray.origin -= normal * EPSILON;
	}

	ray.color += max(vec3(0.0), ray.throughput * direct);
	ray.throughput *= mat.ao;

}

void DirectIllumination(Ray ray, vec3 position, vec3 normal,
	Material mat, vec3 baseColor, out vec3 color) {

	float perceptualRoughness = mat.roughness;
	float metalness = mat.metalness;

	vec3 N = normal;
	vec3 V = normalize(-ray.origin - position);
	vec3 L = -light.direction;
	vec3 H = normalize(L + V);

	float NdotL = saturate(dot(N, L));
	float LdotH = saturate(dot(L, H));
	float NdotH = saturate(dot(N, H));
	float NdotV = saturate(dot(N, V));

	float roughness = perceptualRoughness * perceptualRoughness;

	vec3 f0 = mix(vec3(0.04), vec3(1.0), metalness);
	float f90 = 1.0;

	// Specular BRDF
	vec3 F = FresnelSchlick(f0, f90, LdotH);
	float G = VisibilitySmithGGXCorrelated(NdotV, NdotL, roughness);
	float D = DistributionGGX(NdotH, roughness);
	
	vec3 Fr = D * G * F / PI;

	// Diffuse BRDF
	vec3 Fd = vec3((1.0 - metalness) * RenormalizedDisneyDiffuse(NdotV, NdotL, LdotH, perceptualRoughness) / PI);
	
	float shadowFactor = 1.0;
	if (NdotL > 0.0) {
		// Shadow testing
		ray.direction = -light.direction;
		ray.origin = position + normal * EPSILON;
		ray.inverseDirection = 1.0 / ray.direction;
		shadowFactor = QueryBVH(ray, 0.0, INF) ? 0.0 : 1.0;
	}
	else {
		shadowFactor = 0.0;
	}

	vec3 radiance = light.color * light.intensity;
	color = (Fd + Fr) * radiance * NdotL * shadowFactor;
	
}