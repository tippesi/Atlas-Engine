#include <../raytracer/structures.hsh>
#include <../raytracer/common.hsh>
#include <../raytracer/buffers.hsh>
#include <../raytracer/tracing.hsh>
#include <../common/random.hsh>
#include <../common/flatten.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

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
		
		ray.ID = Flatten2D(pixel, resolution);
		
		ray.direction = normalize(origin + right * coord.x 
			+ bottom * coord.y - cameraLocation);
		ray.origin = cameraLocation;

		ray.hitID = 0;

		// Calculate number of potential overlapping pixels at the borders of a tile
		ivec2 overlappingPixels = tileSize % ivec2(gl_WorkGroupSize);
		// Calculate number of groups that don't have overlapping pixels
		ivec2 perfectGroupCount = tileSize / ivec2(gl_WorkGroupSize);				

		int index = 0;

		ivec2 workGroupId = ivec2(gl_WorkGroupID);

		// Arrange rays in a good way. (E.g. similiar rays in a group)
		if (all(lessThan(workGroupId, perfectGroupCount))) {
			int groupIndex = Flatten2D(ivec2(gl_WorkGroupID), perfectGroupCount);
			index = int(gl_LocalInvocationIndex) + groupIndex * 64;
		}
		else if (workGroupId.x >= perfectGroupCount.x &&
			workGroupId.y < perfectGroupCount.y) {
			int offset = perfectGroupCount.x * perfectGroupCount.y * 64;
			ivec2 localID = ivec2(gl_GlobalInvocationID) - ivec2(perfectGroupCount.x, 0) * ivec2(8);
			index = Flatten2D(localID, overlappingPixels) + offset;
		}
		else {
			int overlappingRight = overlappingPixels.x * int(perfectGroupCount.y) * 8;
			int offset = perfectGroupCount.x * perfectGroupCount.y * 64 + overlappingRight;
			ivec2 localID = ivec2(gl_GlobalInvocationID) - ivec2(0, perfectGroupCount.y) * ivec2(8);
			index = Flatten2D(localID.yx, overlappingPixels.yx) + offset;
		}

		WriteRay(ray, uint(index));
	}

}