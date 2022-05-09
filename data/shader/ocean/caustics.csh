layout (local_size_x = 8, local_size_y = 8) in;

#define SHADOW_FILTER_1x1

#include <../structures>
#include <../shadow.hsh>
#include <../common/convert.hsh>
#include <../common/utility.hsh>

layout(binding = 0) uniform sampler2D depthTexture;
layout(binding = 1, rgba16f) uniform image2D refractionImage;

uniform Light light;
uniform float time;
uniform mat4 ivMatrix;
uniform float waterHeight;

// Modified from Shadertoy: https://www.shadertoy.com/view/XtKfRG
float caustics(vec3 pos) {
    mat3 m = mat3(-2,-1,2, 3,-2,1, 1,2,2);
    vec3 a = vec3(pos.xz * 0.5, time / 4.0) * m;
    vec3 b = a * m * 0.4;
    vec3 c = b * m * 0.3;
    return pow(
          min(min(   length(0.5 - fract(a)), 
                     length(0.5 - fract(b))
                  ), length(0.5 - fract(c)
             )), 7.0) * 25.0;
}

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(refractionImage).x ||
        pixel.y > imageSize(refractionImage).y)
        return;

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(imageSize(refractionImage));

    float depth = textureLod(depthTexture, texCoord, 0.0).r;
    vec3 pixelPos = vec3(ivMatrix * vec4(ConvertDepthToViewSpace(depth, texCoord), 1.0));

    float waterDepth = waterHeight - pixelPos.y;
    if (waterDepth <= 0.0)
        return;

    float shadowFactor = max(CalculateCascadedShadow(light, pixelPos, vec3(0.0, 1.0, 0.0), 1.0), 0.0);

    vec3 pos = vec3(pixelPos.x, time * 0.5, pixelPos.z);
    pos *= 2.0;

    vec3 o = vec3(1.0, 0.0, 1.0)*0.02;
    vec3 w;
    w.x = caustics(pos + o);
    w.y = caustics(pos + o*1.5);
    w.z = caustics(pos + o*2.0);
    
    float shoreScaling = clamp(waterDepth * 0.25, 0.0, 1.0);
    vec3 intensity = w * light.color * light.intensity * shoreScaling * shadowFactor;

    vec3 data = imageLoad(refractionImage, pixel).rgb;

    shadowFactor = saturate(dot(data, vec3(0.333)));
    intensity *= shadowFactor;

    imageStore(refractionImage, pixel, vec4(data + intensity, 0.0));

}