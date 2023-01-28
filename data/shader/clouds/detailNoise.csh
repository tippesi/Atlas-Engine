#include <clouds.hsh>

layout (local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout(set = 3, binding = 0, r16f) uniform image3D noiseImage;

layout(push_constant) uniform constants {
	float seed;
} pushConstants;

void main() {

    ivec3 size = imageSize(noiseImage);
    ivec3 pixel = ivec3(gl_GlobalInvocationID);

    vec3 pos = vec3(pixel) / vec3(size);

    float baseScale = 3.0;
    vec4 weights = vec4(1.0, 0.5, 0.25, 0.125);

    vec4 noise;
    noise.r = Worley4Octaves(pos, 1.0 * baseScale, pushConstants.seed, weights);
    noise.g = Worley4Octaves(pos, 2.0 * baseScale, pushConstants.seed, weights);
    noise.b = Worley4Octaves(pos, 4.0 * baseScale, pushConstants.seed, weights);
    noise.a = Worley4Octaves(pos, 8.0 * baseScale, pushConstants.seed, weights);

    float highFrequenyFBM = noise.r * 0.625
                            + noise.g * 0.250
                            + noise.b * 0.125;

    imageStore(noiseImage, pixel, vec4(highFrequenyFBM, 0.0, 0.0, 0.0));

}