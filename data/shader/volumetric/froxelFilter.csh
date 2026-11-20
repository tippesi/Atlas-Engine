layout (local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

#include <../globals.hsh>
#include <../common/convert.hsh>

#include <froxel.hsh>

layout(set = 3, binding = 0, rgba16f) writeonly uniform image3D froxelImage;
layout(set = 3, binding = 1) uniform sampler3D froxelTexture;

const float weights[7] = float[](0.071303, 0.131514, 0.189879, 0.214607, 0.189879, 0.131514, 0.071303);

void main() {

    ivec3 froxel = ivec3(gl_GlobalInvocationID);
    ivec3 froxelCount = ivec3(pushConstants.columnCount, pushConstants.sliceCount);
    if (any(greaterThanEqual(froxel, froxelCount)))
        return;

#ifdef HORIZONTAL
    ivec3 offset = ivec3(1, 0, 0);
#else
    ivec3 offset = ivec3(0, 1, 0);
#endif

    vec4 result = vec4(0.0);
    for (int i = -3; i <= 3; i++) {
        ivec3 coord = clamp(froxel + i * offset, ivec3(0), froxelCount - 1);
        result += weights[i + 3] * texelFetch(froxelTexture, coord, 0);
    }

    imageStore(froxelImage, froxel, result);

}
