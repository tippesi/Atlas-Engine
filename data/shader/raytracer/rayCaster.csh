#include <structures>
#include <intersections>

layout (local_size_x = 16, local_size_y = 16) in;

layout (binding = 0, rgba8) writeonly uniform image2D outputImage;

layout (std430, binding = 1) buffer Triangles {
	Triangle data[];
} triangles;

layout (std430, binding = 2) buffer MaterialIndices {
	int data[];
} materialIndices;

layout (std430, binding = 3) buffer Materials {
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
const int sharedDataCount = 32;

shared vec3 sharedVertices[sharedDataCount * 3];

void EvaluateLight(int triangleIndex, vec2 barrycentric, out vec3 color);

void syncronize() {
	memoryBarrierShared();
	barrier();
}

void main() {

	vec2 coord = vec2(gl_GlobalInvocationID.xy) / 
		vec2(float(width), float(height));
		
	Ray ray;
	
	ray.direction = normalize(origin + right * coord.x 
		+ bottom * coord.y - cameraLocation);
	ray.inverseDirection = 1.0 / ray.direction;
	ray.origin = cameraLocation + cameraNearPlane * ray.direction;
	
	float intersectionDistance = cameraFarPlane;
	int triangleIndex = 0;
	vec2 barrycentric = vec2(0.0);	

	for (int i = 0; i < triangleCount; i+=sharedDataCount) {
	
		// One thread copies a data chunk to shared memory
		if (gl_LocalInvocationID.x == 0 &&
			gl_LocalInvocationID.y == 0) {
			for (int j = 0; j < sharedDataCount; j++) {
				if (i + j < triangleCount) {
					sharedVertices[j * 3] = triangles.data[i + j].v0.xyz;
					sharedVertices[j * 3 + 1] = triangles.data[i + j].v1.xyz;
					sharedVertices[j * 3 + 2] = triangles.data[i + j].v2.xyz;
				}
				else {
					sharedVertices[j * 3] = triangles.data[triangleCount - 1].v0.xyz;
					sharedVertices[j * 3 + 1] = triangles.data[triangleCount - 1].v1.xyz;
					sharedVertices[j * 3 + 2] = triangles.data[triangleCount - 1].v2.xyz;
				}
			}
		}
		
		syncronize();
		
		for (int j = 0; j < sharedDataCount; j++) {
	
			vec3 v0 = sharedVertices[j * 3];
			vec3 v1 = sharedVertices[j * 3 + 1];
			vec3 v2 = sharedVertices[j * 3 + 2];
			
			vec3 sol;
			if (!Intersection(ray, v0, v1, v2, sol))
				continue;
			
			if (sol.x >= intersectionDistance)
				continue;
				
			intersectionDistance = sol.x;
			triangleIndex = i + j;
			barrycentric = sol.yz;
			
		}
	}
	
	vec3 color;
	
	if (intersectionDistance < cameraFarPlane) {
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

	// Interpolate normal by using barrycentric coordinates
	vec3 normal = vec3((1.0 - barrycentric.x - barrycentric.y) * triangle.n0 + 
		barrycentric.x * triangle.n1 + barrycentric.y * triangle.n2);

	vec3 surfaceColor = materials.data[materialIndices.data[triangleIndex]].diffuseColor.rgb;
	
	vec3 specular = vec3(0.0);
	vec3 diffuse = vec3(1.0);
	vec3 ambient = vec3(light.ambient * surfaceColor);
	
	diffuse = max((dot(normal, -light.direction) * light.color), ambient) * surfaceColor;		
	
	color = diffuse + specular + ambient;
	
	color = pow(color, vec3(gamma));

}