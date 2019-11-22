#include <structures>
#include <intersections>
#include <texture>
#include <random>
#include <BVH>
#include <../common/packing>
#include <../common/PI>

#define INF 1000000000000.0

layout (local_size_x = 8, local_size_y = 8) in;

layout (binding = 0, rgba8) writeonly uniform image2D outputImage;

/*
Except for image variables qualified with the format qualifiers r32f, r32i, and r32ui, 
image variables must specify either memory qualifier readonly or the memory qualifier writeonly.
Reading and writing simultaneously to other formats is not supported on OpenGL ES
*/
layout (binding = 1, rgba32f) readonly uniform image2D inAccumImage;
layout (binding = 2, rgba32f) writeonly uniform image2D outAccumImage;

layout (std430, binding = 1) buffer Materials {
	Material data[];
} materials;

// Size of the image
uniform int width;
uniform int height;

// Far plane for ray calculation
uniform vec3 origin;
uniform vec3 right;
uniform vec3 bottom;

// Camera settings
uniform vec3 cameraLocation;
uniform float cameraFarPlane;
uniform float cameraNearPlane;

// Triangle count
uniform int triangleCount;

// Light
uniform Light light;

// Total sample count
uniform int sampleCount;
uniform ivec2 pixelOffset;

const float gamma = 1.0 / 2.2;

void Radiance(Ray ray, vec2 coord, out vec3 color);
void DirectIllumination(vec3 position, vec3 normal, Material material, out vec3 color);

Triangle UnpackTriangle(PackedTriangle triangle);

void main() {
	
	ivec2 pixel = ivec2(gl_GlobalInvocationID.xy) + pixelOffset;
	
	float jitterX = random(vec2(float(sampleCount), 0.0));
	float jitterY = random(vec2(float(sampleCount), 1.0));

	vec2 coord = (vec2(pixel) + vec2(jitterX, jitterY)) / 
		vec2(float(width), float(height));
		
	vec3 color = vec3(0.0);
	
	Ray ray;
	
	ray.direction = normalize(origin + right * coord.x 
		+ bottom * coord.y - cameraLocation);
	ray.inverseDirection = 1.0 / ray.direction;
	ray.origin = cameraLocation + cameraNearPlane * ray.direction;
	
	Radiance(ray, coord, color);
		
	vec4 accumColor = imageLoad(inAccumImage, pixel);
		
	accumColor += vec4(color, 1.0);
	
	imageStore(outAccumImage, pixel, accumColor);

	imageStore(outputImage, pixel,
		pow(accumColor / float(sampleCount), vec4(gamma)));

}

void Radiance(Ray ray, vec2 coord, out vec3 color) {

	vec3 mask = vec3(1.0);
	vec3 accumColor = vec3(0.0);

	for (int bounces = 0; bounces < 3; bounces++) {

		int triangleIndex = 0;
		vec3 intersection;
		vec2 barrycentric = vec2(0.0);

		QueryBVH(ray, 0.0, INF, triangleIndex, intersection);
		barrycentric = intersection.yz;
		
		if (intersection.x >= INF) {			
			color = vec3(0.0);
			return;			
		}
		
		Triangle tri = UnpackTriangle(triangles.data[triangleIndex]);
		Material mat = materials.data[tri.materialIndex];
		
		float s = barrycentric.x;
		float t = barrycentric.y;
		float r = 1.0 - s - t;
		
		// Interpolate normal by using barrycentric coordinates
		vec3 position = ray.origin + intersection.x * ray.direction;
		vec2 texCoord = r * tri.uv0 + s * tri.uv1 + t * tri.uv2;
		vec3 normal = normalize(r * tri.n0 + s * tri.n1 + t * tri.n2);
			
		// Produces some problems in the bottom left corner of the Sponza scene,
		// but fixes the cube. Should work in theory.
		normal = dot(normal, ray.direction) <= 0.0 ? normal : normal * -1.0;
		
		vec3 surfaceColor = vec3(mat.diffR, mat.diffG, mat.diffB) * 
			vec3(SampleDiffuseBilinear(mat.diffuseTexture, texCoord));
			
		mat3 toTangentSpace = mat3(tri.t, tri.bt, normal);		
			
		// Sample normal map
		if (mat.normalTexture.layer >= 0) {
			normal = normalize(toTangentSpace * 
				(2.0 * vec3(SampleNormalBilinear(mat.normalTexture, texCoord)) - 1.0));		
		}
			
		vec3 emissiveColor = vec3(mat.emissR, mat.emissG, mat.emissB);
		
		vec3 direct;
		DirectIllumination(position, normal, mat, direct);
		
		// create 2 random numbers
		float r1 = 2.0 * PI * random(vec4(coord, float(sampleCount), float(bounces)));
		float r2 = random(vec4(coord, float(sampleCount), float(bounces) + 1.0));
		float r2s = sqrt(r2);

		// compute orthonormal coordinate frame uvw with hitpoint as origin 
		vec3 w = normal;
		vec3 u = normalize(cross((abs(w.x) > .1 ? vec3(0.0, 1.0, 0.0)
			: vec3(1.0, 0.0, 0.0)), w));
		vec3 v = cross(w, u);
		
		ray.origin = position;
		ray.direction = normalize(u*cos(r1)*r2s + v*sin(r1)*r2s + w*sqrt(1.0 - r2));
		ray.inverseDirection = 1.0 / ray.direction;
		
		ray.origin += normal * 0.03;
		
		
		accumColor += (surfaceColor * direct + emissiveColor) * mask;
		
		mask *= surfaceColor;
		mask *= dot(ray.direction, normal);		
		
	}
	
	color = accumColor;

}

void DirectIllumination(vec3 position, vec3 normal,
	Material mat, out vec3 color) {
	
	float shadowFactor = 1.0;
	
	// Shadow testing
	Ray ray;
	ray.direction = -light.direction;
	ray.origin = position + normal * 0.01;
	ray.inverseDirection = 1.0 / ray.direction;
	
	vec3 intersection;
	int triangleIndex = 0;
	QueryBVH(ray, 0.0, INF, triangleIndex, intersection);
	shadowFactor = intersection.x < INF ? 0.0 : 1.0;
	
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

Triangle UnpackTriangle(PackedTriangle triangle) {
	
	Triangle tri;

	tri.v0 = triangle.v0.xyz;
	tri.v1 = triangle.v1.xyz;
	tri.v2 = triangle.v2.xyz;
	
	tri.n0 = vec3(unpackUnitVector(floatBitsToInt(triangle.v0.w)));
	tri.n1 = vec3(unpackUnitVector(floatBitsToInt(triangle.v1.w)));
	tri.n2 = vec3(unpackUnitVector(floatBitsToInt(triangle.v2.w)));	
	
	tri.t = vec3(unpackUnitVector(floatBitsToInt(triangle.d1.x)));
	tri.bt = vec3(unpackUnitVector(floatBitsToInt(triangle.d1.y)));
	
	tri.uv0 = unpackHalf2x16(floatBitsToUint(triangle.d0.x));
	tri.uv1 = unpackHalf2x16(floatBitsToUint(triangle.d0.y));
	tri.uv2 = unpackHalf2x16(floatBitsToUint(triangle.d0.z));
	
	tri.materialIndex = floatBitsToInt(triangle.d0.w);
	
	return tri;
	
}