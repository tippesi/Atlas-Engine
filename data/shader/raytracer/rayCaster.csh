#include <structures>
#include <intersections>
#include <BVH>

layout (local_size_x = 8, local_size_y = 8) in;

layout (binding = 0, rgba8) writeonly uniform image2D outputImage;

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

const float gamma = 1.0 / 2.2;

void EvaluateLight(int triangleIndex, vec2 barrycentric, out vec3 color);

void main() {

	vec2 coord = vec2(gl_GlobalInvocationID.xy) / 
		vec2(float(width), float(height));
		
	Ray ray;
	
	ray.direction = normalize(origin + right * coord.x 
		+ bottom * coord.y - cameraLocation);
	ray.inverseDirection = 1.0 / ray.direction;
	ray.origin = cameraLocation + cameraNearPlane * ray.direction;
	
	int triangleIndex = 0;
	vec3 intersection;
	vec2 barrycentric = vec2(0.0);

	QueryBVH(ray, 0.0, cameraFarPlane, triangleIndex, intersection);
	barrycentric = intersection.yz;
	
	vec3 color;
	
	if (intersection.x < cameraFarPlane) {
		EvaluateLight(triangleIndex, barrycentric, color);
	}
	else {
		color = vec3(0.0);
	}

	imageStore(outputImage, ivec2(gl_GlobalInvocationID.xy),
		vec4(color, 1.0));

}

void EvaluateLight(int triangleIndex, vec2 barrycentric, out vec3 color) {

	Triangle triangle = triangles.data[triangleIndex];
	Material mat = materials.data[materialIndices.data[triangleIndex]];

	// Interpolate normal by using barrycentric coordinates
	vec3 normal = normalize(vec3((1.0 - barrycentric.x - barrycentric.y) * triangle.n0 + 
		barrycentric.x * triangle.n1 + barrycentric.y * triangle.n2));
	vec3 position = vec3((1.0 - barrycentric.x - barrycentric.y) * triangle.v0 + 
		barrycentric.x * triangle.v1 + barrycentric.y * triangle.v2);
	vec3 surfaceColor = vec3(mat.diffR, mat.diffG, mat.diffB);
	
	float shadowFactor = 1.0;
	/*
	Shadow testing
	Ray ray;
	ray.direction = -light.direction;
	ray.origin = position -light.direction * 0.0001;
	ray.inverseDirection = 1.0 / ray.direction;
	
	vec3 intersection;
	QueryBVH(ray, 0.0, cameraFarPlane, triangleIndex, intersection);
	shadowFactor = intersection.x < cameraFarPlane ? 0.0 : 1.0;
	*/
	
	vec3 viewDir = normalize(cameraLocation - position);
	vec3 lightDir = -light.direction;
	
	float nDotL = max((dot(normal, lightDir)), 0.0);
	
	vec3 specular = vec3(1.0);
	vec3 diffuse = surfaceColor;
	vec3 ambient = vec3(light.ambient * surfaceColor);
	
	vec3 halfway = normalize(lightDir + viewDir);
	specular *= pow(max(dot(normal, halfway), 0.0), mat.specularHardness)
		* mat.specularIntensity;
	
	color = (diffuse + specular) * nDotL * light.color * shadowFactor + ambient;
	
	color = pow(color, vec3(gamma));

}