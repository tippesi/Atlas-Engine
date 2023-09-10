layout (local_size_x = 8, local_size_y = 8) in;

#define SHADOW_FILTER_3x3

#include <common.hsh>
#include <sharedUniforms.hsh>

#include <../structures>
#include <../shadow.hsh>
#include <../globals.hsh>
#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../common/stencil.hsh>
#include <../common/normalencode.hsh>

layout (set = 3, binding = 4) uniform sampler2D refractionTexture;
layout(set = 3, binding = 16) uniform sampler2D depthTexture;
layout(set = 3, binding = 17) uniform usampler2D stencilTexture;
layout(set = 3, binding = 18) uniform sampler2D oceanDepthTexture;
layout(set = 3, binding = 19) uniform sampler2D oceanNormalTexture;
layout(set = 3, binding = 8) uniform sampler2DArrayShadow cascadeMaps;
layout(set = 3, binding = 20, rgba16f) uniform image2D outputImage;


void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(outputImage).x ||
        pixel.y > imageSize(outputImage).y)
        return;

    Light light = LightUniforms.light;

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(imageSize(outputImage));

    float depth = textureLod(depthTexture, texCoord, 0.0).r;

    float oceanDepth = textureLod(oceanDepthTexture, texCoord, 0.0).r;
    vec3 viewSpacePos = ConvertDepthToViewSpace(depth, texCoord);
    vec3 viewSpaceOceanPos = ConvertDepthToViewSpace(oceanDepth, texCoord);
    vec3 pixelPos = vec3(globalData.ivMatrix * vec4(viewSpacePos, 1.0));
    vec3 oceanPos = vec3(globalData.ivMatrix * vec4(viewSpaceOceanPos, 1.0));
    vec3 nearPos = vec3(globalData.ivMatrix * vec4(ConvertDepthToViewSpace(0.0, texCoord), 1.0));

    float distanceToCamera = length(viewSpacePos);

    StencilFeatures features = DecodeStencilFeatures(texelFetch(stencilTexture, pixel, 0).r);
    
    float perlinScale, shoreScaling;
    vec3 normalShoreWave;
    vec3 displacement = GetOceanDisplacement(nearPos, 0.0, perlinScale, shoreScaling, normalShoreWave);
    
    vec3 viewVector = pixelPos - nearPos;

    float waterDepth = Uniforms.translation.y - nearPos.y + displacement.y;
    if (waterDepth < 0.0 && depth < oceanDepth && oceanDepth < 1.0 ||
        oceanDepth == 1.0 && pixelPos.y > Uniforms.translation.y ||
        (!features.underWaterPixel && features.waterPixel))
        return;

    texCoord = (texCoord + vec2(0.05)) * 0.9;

    texCoord.x += sin(texCoord.y * 2.0 * PI * 5.0 + 5.0 * globalData.time) * 0.001 / depth;
    texCoord.y += cos(texCoord.x * 2.0 * PI * 2.0 + 2.0 * globalData.time) * 0.001 / depth;

    depth = textureLod(depthTexture, texCoord, 0.0).r;
    viewSpacePos = ConvertDepthToViewSpace(depth, texCoord);
    pixelPos = vec3(globalData.ivMatrix * vec4(viewSpacePos, 1.0));

    vec3 refractionColor = textureLod(refractionTexture, texCoord, 0.0).rgb;

    float NDotL = dot(-light.direction.xyz, vec3(0.0, 1.0, 0.0));
    
    viewVector = normalize(viewVector);

    float waterOffset = max(0.0, (nearPos.y - Uniforms.translation.y) / max(viewVector.y, 0.0001));
    float waterViewDepth = distance(pixelPos, nearPos);

    // Calculate water color
    vec3 depthFog = mix(Uniforms.deepWaterBodyColor.rgb, Uniforms.waterBodyColor.rgb, min(1.0 , exp(-waterViewDepth / 20.0)));
    float diffuseFactor = Uniforms.waterColorIntensity.x + Uniforms.waterColorIntensity.y * max(0.0, NDotL);
    vec3 waterColor = diffuseFactor * light.intensity * light.color.rgb * depthFog;

    // Update refraction color based on water depth (exponential falloff)
    refractionColor = mix(waterColor, refractionColor, min(1.0 , exp(-waterViewDepth / 5.0)));

    imageStore(outputImage, pixel, vec4(refractionColor, 0.0));

}