#include <../globals.hsh>
#include <../raytracer/structures.hsh>
#include <../raytracer/common.hsh>
#include <../raytracer/buffers.hsh>
#include <../raytracer/tracing.hsh>
#include <../common/random.hsh>
#include <../common/flatten.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

#ifdef REALTIME
layout (set = 3, binding = 1, r32ui) writeonly uniform uimage2DArray frameAccumImage;
#endif

layout(set = 3, binding = 4) uniform UniformBuffer {
    vec4 origin;
    vec4 right;
    vec4 bottom;
    ivec2 pixelOffset;
    ivec2 resolution;
    ivec2 tileSize;
    int sampleCount;
} Uniforms;

void main() {

    if (int(gl_GlobalInvocationID.x) < Uniforms.tileSize.x &&
        int(gl_GlobalInvocationID.y) < Uniforms.tileSize.y) {

        ivec2 pixel = ivec2(gl_GlobalInvocationID.xy) + Uniforms.pixelOffset;
        
        // Apply a subpixel jitter to get supersampling
        float jitterX = random(vec2(float(Uniforms.sampleCount), 0.0));
        float jitterY = random(vec2(float(Uniforms.sampleCount), 1.0));
#ifndef REALTIME
        vec2 coord = (vec2(pixel) + vec2(jitterX, jitterY)) / 
            vec2(float(Uniforms.resolution.x), float(Uniforms.resolution.y));
#else
        vec2 coord = globalData.jitterCurrent * 0.5 + (vec2(pixel) + vec2(0.5)) / 
            vec2(float(Uniforms.resolution.x), float(Uniforms.resolution.y));
#endif
        
        Ray ray;
        
        ray.ID = Flatten2D(pixel, Uniforms.resolution) * int(gl_NumWorkGroups.z) + int(gl_WorkGroupID.z);
        
        ray.direction = normalize(Uniforms.origin.xyz + Uniforms.right.xyz * coord.x 
            + Uniforms.bottom.xyz * coord.y - globalData.cameraLocation.xyz);
        ray.origin = globalData.cameraLocation.xyz;

        ray.hitID = 0;

        // Calculate number of potential overlapping pixels at the borders of a tile
        ivec2 overlappingPixels = Uniforms.tileSize % ivec2(gl_WorkGroupSize);
        // Calculate number of groups that don't have overlapping pixels
        ivec2 perfectGroupCount = Uniforms.tileSize / ivec2(gl_WorkGroupSize);                

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

        WriteRay(ray, uint(index) * gl_NumWorkGroups.z + gl_WorkGroupID.z);
        
#ifdef REALTIME
        if (gl_WorkGroupID.z == 0u) {
            imageStore(frameAccumImage, ivec3(pixel, 0), uvec4(0u));
            imageStore(frameAccumImage, ivec3(pixel, 1), uvec4(0u));
            imageStore(frameAccumImage, ivec3(pixel, 2), uvec4(0u));
        }
#endif

    }

}