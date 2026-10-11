#include <clouds.hsh>

layout (local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout(set = 3, binding = 0, rgba16f) uniform image3D noiseImage;

layout(push_constant) uniform constants {
    float seed;
} pushConstants;

void main() {

    ivec3 size = imageSize(noiseImage);
    ivec3 pixel = ivec3(gl_GlobalInvocationID);

    vec3 pos = vec3(pixel) / vec3(size);

    // Four cells in the first octave still leave enough voxels per feature in a 32^3 texture
    float baseScale = 4.0;

    float worley0 = saturate(Worley(pos, baseScale, pushConstants.seed));
    float worley1 = saturate(Worley(pos, baseScale * 2.0, pushConstants.seed));
    float worley2 = saturate(Worley(pos, baseScale * 4.0, pushConstants.seed));
    float worley3 = saturate(Worley(pos, baseScale * 8.0, pushConstants.seed));

    imageStore(noiseImage, pixel, vec4(worley0, worley1, worley2, worley3));

}
