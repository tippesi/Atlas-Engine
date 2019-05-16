#include <structures>
#include <intersections>

layout (local_size_x = 16, local_size_y = 16) in;

layout (binding = 0, rgba8) writeonly uniform image2D outputImage;

layout (std430, binding = 1) buffer Triangles {
	Triangle data[];
} triangles;

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

void main() {

	vec2 coord = vec2(gl_GlobalInvocationID.xy) / 
		vec2(float(width), float(height));
		
	Ray ray;
	
	ray.direction = normalize(origin + right * coord.x 
		+ bottom * coord.y - cameraLocation);
	ray.inverseDirection = 1.0 / ray.direction;
	ray.origin = cameraLocation + cameraNearPlane * ray.direction;
	
	vec3 intersectionPoint = cameraFarPlane * ray.direction;
	float intersectionDistance = cameraFarPlane;

	for (int i = 0; i < triangleCount; i++) {
		vec3 v0 = triangles.data[i].v0.xyz;
		vec3 v1 = triangles.data[i].v1.xyz;
		vec3 v2 = triangles.data[i].v2.xyz;
		
		vec3 temp;
		float t;
		if (!Intersection(ray, v0, v1, v2, t, temp))
			continue;
		
		if (t >= intersectionDistance)
			continue;
			
		intersectionDistance = t;
		intersectionPoint = temp;		
	}
	
	float gradient = (cameraFarPlane - intersectionDistance) / cameraFarPlane;

	imageStore(outputImage, ivec2(gl_GlobalInvocationID.xy),
		vec4(vec3(gradient), 1.0));

}