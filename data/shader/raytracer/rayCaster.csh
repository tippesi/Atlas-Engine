#include <structures>
#include <intersections>
#include <texture>
#include <random>
#include <BVH>
#include <../common/PI>

layout (local_size_x = 8, local_size_y = 8) in;

layout (binding = 0, rgba8) writeonly uniform image2D outputImage;
layout (binding = 1, rgba32f) uniform image2D accumulationImage;

#define INF 1000000000000.0

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

const float gamma = 1.0 / 2.2;

void Radiance(Ray ray, vec2 coord, out vec3 color);
void DirectIllumination(vec3 position, vec3 normal,
	vec2 texCoord, Material material, out vec3 color);

void main() {

	vec2 coord = vec2(gl_GlobalInvocationID.xy) / 
		vec2(float(width), float(height));
		
	vec3 color = vec3(0.0);
	
	Ray ray;
	
	ray.direction = normalize(origin + right * coord.x 
		+ bottom * coord.y - cameraLocation);
	ray.inverseDirection = 1.0 / ray.direction;
	ray.origin = cameraLocation + cameraNearPlane * ray.direction;
	
	Radiance(ray, coord, color);
		
	vec4 accumColor = imageLoad(accumulationImage,
		ivec2(gl_GlobalInvocationID.xy));
		
	accumColor += vec4(color, 1.0);
	
	imageStore(accumulationImage, ivec2(gl_GlobalInvocationID.xy), accumColor);

	imageStore(outputImage, ivec2(gl_GlobalInvocationID.xy),
		pow(accumColor / float(sampleCount), vec4(gamma)));

}

void Radiance(Ray ray, vec2 coord, out vec3 color) {

	vec3 mask = vec3(1.0);
	vec3 accumColor = vec3(0.0);

	for (int bounces = 0; bounces < 4; bounces++) {

		int triangleIndex = 0;
		vec3 intersection;
		vec2 barrycentric = vec2(0.0);

		QueryBVH(ray, 0.0, INF, triangleIndex, intersection);
		barrycentric = intersection.yz;
		
		if (intersection.x >= INF) {			
			color = vec3(0.0);
			return;			
		}
		
		Triangle triangle = triangles.data[triangleIndex];
		Material mat = materials.data[materialIndices.data[triangleIndex]];
		
		vec2 t0 = vec2(triangle.v0.w, triangle.v1.w);
		vec2 t1 = vec2(triangle.v2.w, triangle.n0.w);
		vec2 t2 = vec2(triangle.n1.w, triangle.n2.w);

		// Interpolate normal by using barrycentric coordinates
		vec3 normal = normalize(vec3((1.0 - barrycentric.x - barrycentric.y) * triangle.n0 + 
			barrycentric.x * triangle.n1 + barrycentric.y * triangle.n2));
		vec3 position = vec3((1.0 - barrycentric.x - barrycentric.y) * triangle.v0 + 
			barrycentric.x * triangle.v1 + barrycentric.y * triangle.v2);
		vec2 texCoord = (1.0 - barrycentric.x - barrycentric.y) * t0 + 
			barrycentric.x * t1 + barrycentric.y * t2;
			
		normal = dot(normal, ray.direction) < 0.0 ? normal : normal * -1.0;
		
		vec3 surfaceColor = vec3(mat.diffR, mat.diffG, mat.diffB) *
			vec3(SampleDiffuseBilinear(mat, texCoord));
		
		vec3 direct;
		DirectIllumination(position, normal, texCoord, mat, direct);
		
		// create 2 random numbers
		float r1 = 2.0 * PI * random(vec4(coord, float(sampleCount), 0));
		float r2 = random(vec4(coord, float(sampleCount), 1));
		float r2s = sqrt(r2);

		// compute orthonormal coordinate frame uvw with hitpoint as origin 
		vec3 w = normal;
		vec3 u = normalize(cross((abs(w.x) > .1 ? vec3(0, 1, 0) : vec3(1, 0, 0)), w));
		vec3 v = cross(w, u);
		
		ray.origin = position;
		ray.direction = normalize(u*cos(r1)*r2s + v*sin(r1)*r2s + w*sqrt(1 - r2));
		ray.inverseDirection = 1.0 / ray.direction;
		
		ray.origin += normal * 0.03;
		
		mask *= surfaceColor;
		
		accumColor += direct * mask;
		mask *= dot(ray.direction, normal);		
		
	}
	
	color = accumColor;

}

void DirectIllumination(vec3 position, vec3 normal,
	vec2 texCoord, Material mat, out vec3 color) {
	
	float shadowFactor = 1.0;	
	
	// Shadow testing
	Ray ray;
	ray.direction = -light.direction;
	ray.origin = position -light.direction * 0.0001;
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