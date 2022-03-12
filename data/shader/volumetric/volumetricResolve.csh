#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <fog.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(binding = 0) uniform sampler2D volumetricTexture;
layout(binding = 1) uniform sampler2D depthTexture;
layout(binding = 0, rgba16f) uniform image2D resolveImage;

uniform mat4 ivMatrix;
uniform vec3 cameraLocation;

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(resolveImage).x ||
        pixel.y > imageSize(resolveImage).y)
        return;

	vec2 texCoord = (vec2(pixel) + 0.5) / vec2(imageSize(resolveImage));

	float depth = texelFetch(depthTexture, pixel, 0).r;

	vec3 viewPosition = ConvertDepthToViewSpace(depth, texCoord);
	vec3 worldPosition = vec3(ivMatrix * vec4(viewPosition, 1.0));

    vec4 resolve = imageLoad(resolveImage, pixel);
	vec4 volumetric = vec4(textureLod(volumetricTexture, texCoord, 0).rgb, 0.0);

	float fogAmount = fogEnabled ? saturate(ComputeVolumetricFog(cameraLocation, worldPosition)) : 0.0;
    resolve = fogEnabled ? mix(vec4(fogColor, 1.0), resolve, fogAmount) + volumetric : resolve + volumetric;

    imageStore(resolveImage, pixel, resolve);
}