#include <globals.hsh>
#include <common/convert.hsh>
#include <common/PI.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(set = 3, binding = 0, rgba16f) writeonly uniform image2D colorImage;
layout(set = 3, binding = 1, rg16f) writeonly uniform image2D velocityImage;
layout(set = 3, binding = 2) uniform sampler2D depthTexture;
layout(set = 3, binding = 3) uniform samplerCube skyCubemap;

layout(push_constant) uniform constants {
	vec4 cameraLocationLast;
} pushConstants;

vec2 resolution = vec2(imageSize(colorImage));

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(colorImage).x ||
        pixel.y > imageSize(colorImage).y)
        return;

    float depth = texelFetch(depthTexture, pixel, 0).r;
    if (depth < 1.0)
        return;

    vec2 texCoord = (vec2(pixel) + 0.5) / resolution;

    // Don't use the global inverse matrices here, since we also render the cubemap with this shader
	vec3 viewPos = ConvertDepthToViewSpace(depth, texCoord);
    vec3 worldPos = vec3(globalData.ivMatrix * vec4(viewPos, 1.0));

    vec3 cubemapCoord = normalize(worldPos - globalData.cameraLocation.xyz);

    vec3 color = textureLod(skyCubemap, cubemapCoord, 0).rgb;

    vec3 cameraLocationDiff = globalData.cameraLocation.xyz - pushConstants.cameraLocationLast.xyz;

    // Calculate velocity
    vec3 ndcCurrent = (globalData.pMatrix * vec4(viewPos, 1.0)).xyw;
    vec3 ndcLast = (globalData.pvMatrixLast * vec4(worldPos - cameraLocationDiff, 1.0)).xyw;

    vec2 ndcL = ndcLast.xy / ndcLast.z;
    vec2 ndcC = ndcCurrent.xy / ndcCurrent.z;

    ndcL -= globalData.jitterLast;
    ndcC -= globalData.jitterCurrent;

    vec2 velocity = (ndcL - ndcC) * 0.5;

    imageStore(velocityImage, pixel, vec4(velocity, 0.0, 1.0));
    imageStore(colorImage, pixel, vec4(color, 1.0));

}