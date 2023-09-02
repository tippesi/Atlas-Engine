layout (local_size_x = 8, local_size_y = 8) in;

#define SHADOW_FILTER_3x3

#include <sharedUniforms.hsh>

#include <../structures>
#include <../shadow.hsh>
#include <../globals.hsh>
#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../common/stencil.hsh>

layout(set = 3, binding = 0) uniform sampler2D depthTexture;
layout(set = 3, binding = 1) uniform usampler2D stencilTexture;
layout(set = 3, binding = 8) uniform sampler2DArrayShadow cascadeMaps;
layout(set = 3, binding = 2, rgba16f) uniform image2D refractionImage;

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(refractionImage).x ||
        pixel.y > imageSize(refractionImage).y)
        return;

    Light light = LightUniforms.light;

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(imageSize(refractionImage));

    float depth = textureLod(depthTexture, texCoord, 0.0).r;
    vec3 viewSpacePos = ConvertDepthToViewSpace(depth, texCoord);
    vec3 pixelPos = vec3(globalData.ivMatrix * vec4(viewSpacePos, 1.0));
    vec3 nearPos = vec3(globalData.ivMatrix * vec4(ConvertDepthToViewSpace(0.0, texCoord), 1.0));

    float distanceToCamera = length(viewSpacePos);

    StencilFeatures features = DecodeStencilFeatures(texelFetch(stencilTexture, pixel, 0).r);

    vec3 viewVector = pixelPos - nearPos;

    float waterDepth = Uniforms.translation.y - pixelPos.y;
    if ((viewVector.y < 0.0 && features.waterPixel == true) || 
        (pixelPos.y > Uniforms.translation.y && features.waterPixel == false))
        return;

    vec3 refractionColor = imageLoad(refractionImage, pixel).rgb;

    //vec3 refractionColor = textureLod(refractionTexture, ndcCoord + refractionDisturbance, 0).rgb;

    float NDotL = dot(-light.direction.xyz, vec3(0.0, 1.0, 0.0));

    float waterViewDepth = -viewSpacePos.z;

    // Calculate water color
    vec3 depthFog = mix(Uniforms.deepWaterBodyColor.rgb, Uniforms.waterBodyColor.rgb, min(1.0 , exp(-waterViewDepth / 20.0)));
    float diffuseFactor = Uniforms.waterColorIntensity.x + Uniforms.waterColorIntensity.y * max(0.0, NDotL);
    vec3 waterColor = diffuseFactor * light.intensity * light.color.rgb * depthFog;

    // Update refraction color based on water depth (exponential falloff)
    refractionColor = mix(waterColor, refractionColor, min(1.0 , exp(-waterViewDepth / 5.0)));

    imageStore(refractionImage, pixel, vec4(refractionColor, 0.0));

}