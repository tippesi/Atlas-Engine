#include <structures>
#include <../common/random>
#include <common>

layout (local_size_x = 8, local_size_y = 8) in;

layout (std430, binding = 4) buffer WriteRays {
	PackedRay writeRays[];
};

// Camera
uniform vec3 cameraLocation;

// Far plane for ray calculation
uniform vec3 origin;
uniform vec3 right;
uniform vec3 bottom;

uniform int sampleCount;
uniform ivec2 pixelOffset;

uniform ivec2 tileSize;
uniform ivec2 resolution;

void main() {

	if (int(gl_GlobalInvocationID.x) < tileSize.x &&
		int(gl_GlobalInvocationID.y) < tileSize.y) {

		ivec2 pixel = ivec2(gl_GlobalInvocationID.xy) + pixelOffset;
		
		// Apply a subpixel jitter to get supersampling
		float jitterX = random(vec2(float(sampleCount), 0.0));
		float jitterY = random(vec2(float(sampleCount), 1.0));

		vec2 coord = (vec2(pixel) + vec2(jitterX, jitterY)) / 
			vec2(float(resolution.x), float(resolution.y));
		
		Ray ray;
		
		ray.pixelID = resolution.x * pixel.y + pixel.x;
		
		ray.direction = normalize(origin + right * coord.x 
			+ bottom * coord.y - cameraLocation);
		ray.origin = cameraLocation;
		
		ray.accumColor = vec3(0.0);
		ray.mask = vec3(1.0);
		
		uint index = tileSize.x * pixel.y + pixel.x;
		writeRays[index] = PackRay(ray);

	}

}