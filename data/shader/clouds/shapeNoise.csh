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

    float baseScale = 6.0;

    float worley0 = saturate(Worley(pos, baseScale, pushConstants.seed));
    float worley1 = saturate(Worley(pos, baseScale * 2.0, pushConstants.seed));
    float worley2 = saturate(Worley(pos, baseScale * 4.0, pushConstants.seed));
    float worley3 = saturate(Worley(pos, baseScale * 8.0, pushConstants.seed));
    float worley4 = saturate(Worley(pos, baseScale * 16.0, pushConstants.seed));

    float worleyFBM0 = worley0 * 0.625 + worley1 * 0.250 + worley2 * 0.125;
    float worleyFBM1 = worley1 * 0.625 + worley2 * 0.250 + worley3 * 0.125;
    float worleyFBM2 = worley2 * 0.625 + worley3 * 0.250 + worley4 * 0.125;

    float perlin = Perlin4Octaves(pos, baseScale,
        vec4(1.0, 0.5, 0.25, 0.125));
    // Remove the low density Worley regions from the connected Perlin structure
    float perlinWorley = saturate(Remap(perlin,
        worleyFBM0, 1.0, 0.0, 1.0));

    // GBA keep the Worley FBMs, such that the density can erode cell boundaries at multiple scales
    imageStore(noiseImage, pixel,
        vec4(perlinWorley, worleyFBM0, worleyFBM1, worleyFBM2));

}
