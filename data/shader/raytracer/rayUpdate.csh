#include <structures>
#include <intersections>
#include <texture>
#include <random>
#include <BVH>
#include <common>

#include <../common/PI>

#define GROUP_SHARED_MEMORY

layout (local_size_x = 64) in;

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

const float gamma = 1.0 / 2.2;

shared uint activeRayMask[4];
shared uint activeRayCount;
shared uint rayOffset;

void Radiance(inout Ray ray, vec2 coord);
void DirectIllumination(vec3 position, vec3 normal, Material material, out vec3 color);

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

		float energy = ray.mask.r + ray.mask.g + ray.mask.b;
		
		if (energy < 1.0 / 1024.0 || bounceCount == 0) {
			if (sampleCount > 0)
				accumColor = imageLoad(inAccumImage, pixel);
				
			accumColor += vec4(ray.accumColor, 1.0);
			
			// Maybe we should substract the jitter to get a sharper image
			imageStore(outAccumImage, pixel, accumColor);

			imageStore(outputImage, pixel,
				pow(accumColor / float(sampleCount + 1), vec4(gamma)));
				
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

	QueryBVHClosest(ray, 0.0, INF, triangleIndex, intersection);
	barrycentric = intersection.yz;
		
	if (intersection.x >= INF) {	
		ray.accumColor += SampleEnvironmentMap(ray.direction).rgb * ray.mask;
		ray.mask = vec3(0.0);
		return;	
	}
		
	Triangle tri = UnpackTriangle(triangles[triangleIndex]);
	Material mat = materials[tri.materialIndex];
		
	vec3 emissiveColor = vec3(mat.emissR, mat.emissG, mat.emissB);
		
	if (length(emissiveColor) > 0.0001) {	
		ray.accumColor += emissiveColor * ray.mask;
		ray.mask = vec3(0.0);
		return;
	}		
		
	float s = barrycentric.x;
	float t = barrycentric.y;
	float r = 1.0 - s - t;
		
	// Interpolate normal by using barrycentric coordinates
	vec3 position = ray.origin + intersection.x * ray.direction;
	vec2 texCoord = r * tri.uv0 + s * tri.uv1 + t * tri.uv2;
	vec3 normal = r * tri.n0 + s * tri.n1 + t * tri.n2;
		
	texCoord = mat.invertUVs ? vec2(texCoord.x, 1.0 - texCoord.y) : texCoord;
	
	// Produces some problems in the bottom left corner of the Sponza scene,
	// but fixes the cube. Should work in theory.
	normal = dot(normal, ray.direction) <= 0.0 ? normal : normal * -1.0;	
		
	vec4 textureColor = SampleDiffuseBilinear(mat.diffuseTexture, texCoord);
	vec3 surfaceColor = vec3(mat.diffR, mat.diffG, mat.diffB) * textureColor.rgb;
			
	mat3 toTangentSpace = mat3(tri.t, tri.bt, normal);		
			
	// Sample normal map
	if (mat.normalTexture.layer >= 0) {
		normal = mix(normal, normalize(toTangentSpace * 
			(2.0 * vec3(SampleNormalBilinear(mat.normalTexture, texCoord)) - 1.0)),
			mat.normalScale);		
	}		
		
	vec3 direct = vec3(0.0);
	DirectIllumination(position, normal, mat, direct);	
		
	ray.origin = position;
	
	float opacity = textureColor.a * mat.opacity;
	float refractChance = 1.0 - opacity;
	float rnd = random(vec4(float(bounceCount + 10), float(sampleCount), coord));
	
	if (rnd >= refractChance) {
		rnd = random(vec4(float(bounceCount + 20), float(sampleCount), coord));
		
		float specChance = mat.specularIntensity;
		
		if (rnd < specChance) {
			vec3 refl = reflect(ray.direction, normal);
			
			ray.direction = HemisphereCos(refl, coord, 
				float(sampleCount), float(bounceCount));
			ray.direction = mix(ray.direction, refl, specChance);
			ray.inverseDirection = 1.0 / ray.direction;
			
			ray.mask *= opacity * specChance;
			ray.origin += normal * EPSILON;
		}
		else {
			ray.direction = HemisphereCos(normal, coord, 
				float(sampleCount), float(bounceCount));
			ray.inverseDirection = 1.0 / ray.direction;
				
			ray.origin += normal * EPSILON;			
			
			ray.accumColor += (surfaceColor * direct * opacity) * ray.mask;
				
			ray.mask *= surfaceColor;
			ray.mask *= dot(ray.direction, normal);
		}
		
	}
	else {
		ray.origin -= normal * EPSILON;
		
		ray.accumColor += (surfaceColor * direct * refractChance) * ray.mask;
		ray.mask *= mix(surfaceColor, vec3(1.0), refractChance);
		
	}

}

void DirectIllumination(vec3 position, vec3 normal,
	Material mat, out vec3 color) {
	
	float shadowFactor = 1.0;
	
	// Shadow testing
	Ray ray;
	ray.direction = -light.direction;
	ray.origin = position + normal * EPSILON;
	ray.inverseDirection = 1.0 / ray.direction;
	
	shadowFactor = QueryBVH(ray, 0.0, INF) ? 0.0 : 1.0;
	
	vec3 viewDir = normalize(cameraLocation - position);
	vec3 lightDir = -light.direction;
	
	float nDotL = max((dot(normal, lightDir)), 0.0);
	
	vec3 specular = vec3(1.0);
	vec3 diffuse = vec3(1.0);
	vec3 ambient = vec3(light.ambient);
	
	vec3 halfway = normalize(lightDir + viewDir);
	specular *= pow(max(dot(normal, halfway), 0.0), mat.specularHardness)
		* mat.specularIntensity;
	
	color = (diffuse) * nDotL * light.color * shadowFactor;
	
}