#include <common/random.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(set = 0, binding = 0, rgba8) uniform image2D image;

layout(push_constant) uniform randomConstant {
	float frameSeed;
} PushConstants;

const float noiseScale = 0.1;

void main() {

	ivec2 pixel = ivec2(gl_GlobalInvocationID);
	if (pixel.x > imageSize(image).x ||
		pixel.y > imageSize(image).y)
		return;

	vec4 color = imageLoad(image, pixel);
	float frameSeed = PushConstants.frameSeed;
	color.rgb += vec3(random(vec2(pixel) / vec2(imageSize(image)), frameSeed) * 2.0 - 1.0) * noiseScale;
	color.rgb = clamp(color.rgb, 0.0, 1.0);

	imageStore(image, pixel, color);

}