#include <structures.hsh>
#include <intersections.hsh>
#include <texture.hsh>
#include <BVH.hsh>
#include <common.hsh>
#include <lights.hsh>

#include <../common/random.hsh>
#include <../common/utility.hsh>
#include <../common/flatten.hsh>
#include <../common/PI.hsh>

#include <../brdf/brdfEval.hsh>
#include <../brdf/importanceSample.hsh>
#include <../brdf/surface.hsh>


// #define GROUP_SHARED_MEMORY

layout (local_size_x = 32) in;

layout (binding = 0, rgba8) writeonly uniform image2D outputImage;

/*
Except for image variables qualified with the format qualifiers r32f, r32i, and r32ui, 
image variables must specify either memory qualifier readonly or the memory qualifier writeonly.
Reading and writing simultaneously to other formats is not supported on OpenGL ES
*/
layout (binding = 1, rgba32f) readonly uniform image2D inAccumImage;
layout (binding = 2, rgba32f) writeonly uniform image2D outAccumImage;

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

layout (std430, binding = 5) buffer Lights {
	PackedLight lights[];
};

layout (std430, binding = 6) buffer Materials {
	RaytraceMaterial materials[];
};

uniform int maxBounces;

uniform int sampleCount;
uniform int bounceCount;
uniform int lightCount;

uniform ivec2 resolution;

uniform float seed;

const float gamma = 1.0 / 2.2;

shared uint activeRayMask[1];
shared uint activeRayCount;
shared uint rayOffset;

void EvaluateBounce(inout Ray ray);

vec3 EvaluateDirectLight(Surface surface, Ray ray);
void EvaluateIndirectLight(Surface surface, inout Ray ray);

bool CheckVisibility(Surface surface, float lightDistance);
Surface GetSurfaceParameters(Triangle triangle, Ray ray, vec3 intersection);

void main() {
	
	uint index = gl_GlobalInvocationID.x;
	uint localIndex = gl_LocalInvocationID.x;
	
	// We can selected whether or not we want to use group shared memory
	// to synchronize the threads and write the rays in a better fashion
	// back into the buffer. From my testing this isn't worth it.
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
		
		ivec2 pixel = Unflatten2D(ray.ID, resolution);
		
		EvaluateBounce(ray);
		
		vec4 accumColor = vec4(0.0);

		float energy = dot(ray.throughput, vec3(1.0));
		
		if (energy == 0 || bounceCount == maxBounces) {
			if (sampleCount > 0)
				accumColor = imageLoad(inAccumImage, pixel);
			
			// Where does it get negative?
			accumColor += vec4(max(ray.color, vec3(0.0)), 1.0);
			
			imageStore(outAccumImage, pixel, accumColor);
			
			vec3 color = accumColor.rgb / float(sampleCount + 1);
			color = vec3(1.0) - exp(-color);
			//color = color / (vec3(1.0) + color);

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

void EvaluateBounce(inout Ray ray) {

	int triangleIndex = 0;
	vec3 intersection;
	vec2 barrycentric = vec2(0.0);

	// Find closest triangle in the BVH
	QueryBVHClosest(ray, 0.0, INF, triangleIndex, intersection);
	
	// If we didn't find a triangle along the ray,
	// we add the contribution of the environment map
	if (intersection.x >= INF) {	
		ray.color += SampleEnvironmentMap(ray.direction).rgb * ray.throughput;
		ray.throughput = vec3(0.0);
		return;	
	}
	
	// Unpack the compressed triangle and extract surface parameters
	Triangle tri = UnpackTriangle(triangles[triangleIndex]);	
	Surface surface = GetSurfaceParameters(tri, ray, intersection);
	
	// If we hit an emissive surface we need to terminate the ray
	if (length(surface.material.emissiveColor) > 0.0 &&
		bounceCount == 0) {
		ray.color += surface.material.emissiveColor;
	}

	// Evaluate direct and indirect light
	vec3 radiance = ray.throughput * EvaluateDirectLight(surface, ray);
	// Clamp indirect radiance
	if (bounceCount > 0) {
		const float radianceLimit = 10.0;
		float radianceMax = max(max(radiance.r, 
			max(radiance.g, radiance.b)), radianceLimit);
		radiance *= radianceLimit / radianceMax;
	}
	ray.color += radiance;
	EvaluateIndirectLight(surface, ray);

}

vec3 EvaluateDirectLight(Surface surface, Ray ray) {

	if (lightCount == 0)
		return vec3(0.0);

	float curSeed = seed;
	float raySeed = float(ray.ID);

	// Weight the lights in the light array based on their approximated
	// contribution to the shading point. Note that we can use a minimum
	// weight for each light to control the variance.
	Light light;
	float totalWeight = 0.0;
	for (int i = 0; i < lightCount; i++) {
		light = UnpackLight(lights[i]);
		float weight = 1.0;
		if (light.type == uint(TRIANGLE_LIGHT)) {
			vec3 pointToLight = light.P - surface.P;
			float sqrDistance = dot(pointToLight, pointToLight);
			float lightDistance = sqrt(sqrDistance);

			vec3 L = normalize(pointToLight);
			float NdotL = abs(dot(light.N, -L));

			weight = light.brightness * light.area * NdotL / sqrDistance;
		}
		else if (light.type == uint(DIRECTIONAL_LIGHT)) {
			weight = light.brightness;
		}
		weight = max(weight, 0.0001);
		totalWeight += weight;
	}
	
	float rnd = random(raySeed, curSeed) * totalWeight;

	float sum = 0.0;

	// We can now find a light based on the random number. Lights
	// with a higher weight will be evaluated more often. To reduce
	// register pressure we recalculate the weights instead of storing
	// them in an array in the first loop.
	float lightPdf = 1.0;
	for (int i = 0; i < lightCount && rnd > sum; i++) {
		light = UnpackLight(lights[i]);
		if (light.type == uint(TRIANGLE_LIGHT)) {		
			vec3 pointToLight = light.P - surface.P;
			float sqrDistance = dot(pointToLight, pointToLight);
			float lightDistance = sqrt(sqrDistance);

			vec3 L = normalize(pointToLight);
			float NdotL = abs(dot(light.N, -L));

			lightPdf = light.brightness * light.area * NdotL / sqrDistance;
		}
		else if (light.type == uint(DIRECTIONAL_LIGHT)) {
			lightPdf = light.brightness;
		}
		lightPdf = max(lightPdf, 0.0001);
		sum += lightPdf;
	}
	
	// Calculate the new probability for the selected light
	lightPdf = lightPdf / totalWeight * light.pdf;

	float solidAngle = 0.0;
	vec3 radiance = light.radiance;
	float lightDistance = INF;

	// Draw a sample on the light and update the surface
	// Here we differentiate between area and punctual lights.
	if (light.type == uint(TRIANGLE_LIGHT)) { // Area lights
		LightSample lightSample;

		float r0 = random(raySeed, curSeed);
		float r1 = random(raySeed, curSeed);

		if (light.type == uint(TRIANGLE_LIGHT)) {
			Triangle tri = UnpackTriangle(triangles[light.idx]);
			lightSample = SampleTriangleLight(light, tri, surface, r0, r1);
		}

		vec3 pointToLight = lightSample.P - surface.P;
		float sqrDistance = dot(pointToLight, pointToLight);
		lightDistance = sqrt(sqrDistance);

		surface.L = pointToLight / lightDistance;
		UpdateSurface(surface);

		float NdotL = abs(dot(lightSample.N, -surface.L));
		solidAngle = NdotL / (sqrDistance * lightSample.pdf);
	
	}
	else  { // Punctual lights
		solidAngle = 1.0;
		if (light.type == uint(DIRECTIONAL_LIGHT)) {
			surface.L = -light.N;
			UpdateSurface(surface);
		}
	}
	
	// Evaluate the BRDF
	vec3 reflectance = EvaluateDiffuseBRDF(surface) + EvaluateSpecularBRDF(surface);
	reflectance *= surface.material.opacity;
	radiance *= solidAngle;

	// Check for visibilty. This is important to get an
	// estimate of the solid angle of the light from point P
	// on the surface.
	if (CheckVisibility(surface, lightDistance) == false)
		radiance = vec3(0.0);
	
	return reflectance * radiance * surface.NdotL / lightPdf;

}

void EvaluateIndirectLight(Surface surface, inout Ray ray) {

	Material mat = surface.material;
	ray.origin = surface.P;

	float curSeed = seed;
	float raySeed = float(ray.ID);

	float refractChance = 1.0 - mat.opacity;
	float rnd = random(raySeed, curSeed);

	float roughness = max(sqr(mat.roughness), 0.00001);

	if (rnd >= refractChance) {
		rnd = random(raySeed, curSeed);

		/*
		float metallicBRDF = mat.metalness;
		float dielectricBRDF = 1.0 - metallicBRDF;

		float specularWeight = metallicBRDF + dielectricBRDF;
		float diffuseWeight = dielectricBRDF;

		float normalization = 1.0 / (specularWeight + diffuseWeight);
		//float specChance = specularWeight * normalization;
		*/

		vec3 F = FresnelSchlick(surface.F0, surface.F90, saturate(dot(-ray.direction, surface.N)));
		float specChance = dot(F, vec3(0.33333));
		
		if (rnd < specChance) {
			vec3 refl = normalize(reflect(ray.direction, surface.N));
			
			float NdotL;
			float pdf;			
			ImportanceSampleCosDir(refl, raySeed, seed, ray.direction,
				NdotL, pdf);			
			ray.direction = mix(refl, ray.direction, roughness);
			pdf = mix(1.0, pdf, roughness);

			ray.inverseDirection = 1.0 / ray.direction;
			ray.origin += surface.N * EPSILON;

			surface.L = ray.direction;
			UpdateSurface(surface);

			ray.throughput *= F / specChance;
			// Need to eliminate the NDF to allow roughness = 0
			//ray.throughput *= EvaluateSpecularBRDF(surface) * surface.NdotL / specChance / pdf;
		}
		else {
			float NdotL;
			float pdf;
			ImportanceSampleCosDir(surface.N, raySeed, seed, ray.direction,
				NdotL, pdf);

			ray.inverseDirection = 1.0 / ray.direction;				
			ray.origin += surface.N * EPSILON;

			surface.L = ray.direction;
			UpdateSurface(surface);
			ray.throughput *= EvaluateDiffuseBRDF(surface) * surface.NdotL / (1.0 - specChance) / pdf;
		}
		
	}
	else {
		ray.throughput *= mix(mat.baseColor, vec3(1.0), refractChance);
	
		ray.origin -= surface.N * EPSILON;
	}

	ray.throughput *= mat.ao;

	// Russain roulette (and avoid division by zero)
	float probability = max(max(ray.throughput.r,
		max(ray.throughput.g, ray.throughput.b)), 0.0001);

	if (random(raySeed, curSeed) > probability) {
		ray.throughput = vec3(0.0);
		return;
	}

	ray.throughput /= probability;

}

bool CheckVisibility(Surface surface, float lightDistance) {

	if (surface.NdotL > 0.0) {
		Ray ray;
		ray.direction = surface.L;
		ray.origin = surface.P + surface.N * EPSILON;
		ray.inverseDirection = 1.0 / ray.direction;
		return QueryBVH(ray, 0.0, lightDistance - 2.0 * EPSILON) == false;
	}
	else {
		return false;
	}

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
		vec3 texNormal = 2.0 * SampleNormalBilinear(rayMat.normalTexture, texCoord) - 1.0;
		texNormal = TBN * texNormal;
		// Make sure we don't divide by zero
		// The normalize function crashes the program in case of vec3(0.0)
		texNormal /= max(length(texNormal), 0.001);
		normal = mix(normal, texNormal, mat.normalScale);		
	}
	
	surface.P = position;
	surface.V = -ray.direction;
	surface.N = normal;
	surface.material = mat;

	surface.F0 = mix(vec3(0.04), surface.material.baseColor,
        surface.material.metalness);
	surface.F90 = 1.0;

	return surface;

}