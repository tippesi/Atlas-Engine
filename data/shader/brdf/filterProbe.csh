#include <../globals.hsh>
#include <../common/random.hsh>
#include <brdf.hsh>
#include <preintegrate.hsh>
#include <filtering.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(set = 3, binding = 0, rgba16f) uniform imageCube filteredCubeMap;
layout(set = 3, binding = 1) uniform samplerCube cubeMap;

layout(push_constant) uniform constants {
    int cubeMapMipLevels;
    float roughness;
    uint mipLevel;
} pushConstants;

vec3 FilterDiffuse(vec3 worldDirection, ivec2 cubeMapSize);
vec3 FilterSpecular(vec3 worldDirection, ivec2 cubeMapSize);

vec3 CubeCoordToWorld(ivec3 cubeCoord, vec2 cubemapSize) {
    vec2 texCoord = (vec2(cubeCoord.xy) + 0.5) / cubemapSize;
    texCoord = texCoord  * 2.0 - 1.0;
    switch(cubeCoord.z) {
        case 0: return vec3(1.0, -texCoord.yx); // posx
        case 1: return vec3(-1.0, -texCoord.y, texCoord.x); //negx
        case 2: return vec3(texCoord.x, 1.0, texCoord.y); // posy
        case 3: return vec3(texCoord.x, -1.0, -texCoord.y); //negy
        case 4: return vec3(texCoord.x, -texCoord.y, 1.0); // posz
        case 5: return vec3(-texCoord.xy, -1.0); // negz
    }
    return vec3(0.0);
}

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(filteredCubeMap).x ||
        pixel.y > imageSize(filteredCubeMap).y)
        return;

    vec3 worldDirection = CubeCoordToWorld(ivec3(gl_GlobalInvocationID),
        vec2(imageSize(filteredCubeMap).xy));

    ivec2 size = textureSize(cubeMap, 0);
    vec3 N = normalize(worldDirection);

#ifdef FILTER_DIFFUSE
    vec3 color = FilterDiffuse(worldDirection, size);
#else
    vec3 color = FilterSpecular(worldDirection, size);
#endif

    imageStore(filteredCubeMap, ivec3(gl_GlobalInvocationID), vec4(color, 0.0));

}

vec3 FilterDiffuse(vec3 worldDirection, ivec2 cubeMapSize) {

    const uint sampleCount = 256u;

    vec4 color = vec4(0.0);

    vec3 N = normalize(worldDirection);

    for (uint i = 0u; i < sampleCount; i++) {
        vec2 Xi = Hammersley(i, sampleCount);

        vec3 L;
        float NdotL;
        float pdf;
        ImportanceSampleCosDir(N, Xi, L, NdotL, pdf);

        NdotL = saturate(NdotL);
        if (NdotL > 0.0) {
            color.xyz += min(textureLod(cubeMap, L, 5.0).rgb, 1.0);
            color.w += 1.0;
        }

    }

    return color.rgb / color.a;

}

vec3 FilterSpecular(vec3 worldDirection, ivec2 cubeMapSize) {

    const uint maxSampleCount = 256u;

    uint sampleCount = uint(mix(16.0, float(maxSampleCount), pushConstants.roughness));
    sampleCount = pushConstants.mipLevel == 0u ? 1u : sampleCount;

    vec4 color = vec4(0.0);

    vec3 N = normalize(worldDirection);
    vec3 V = N;

    for (uint i = 0u; i < sampleCount; i++) {
        vec2 Xi = Hammersley(i, sampleCount);

        vec3 L;
        float NdotL;
        float pdf;
        float alpha = sqr(pushConstants.roughness);
        ImportanceSampleGGX(Xi, N, V, alpha, L, pdf);

        NdotL = dot(N, L);
        vec3 H = normalize(L + V);
        NdotL = saturate(NdotL);

        if (NdotL > 0.0) {
            float NdotH = saturate(dot(N, H));
            float LdotH = saturate(dot(L, H));

            pdf = DistributionGGX(NdotH, alpha) / 4.0;

            float solidAngleTexel = 4.0 * PI / (6.0 * float(cubeMapSize.x * cubeMapSize.y));
            float solidAngleSample = 1.0 / (float(sampleCount) * pdf);
            float lod = clamp(0.5 * log2(float(solidAngleSample / solidAngleTexel)) + 1.0,
                0.0, float(pushConstants.cubeMapMipLevels - 1.0));

            lod = pushConstants.mipLevel == 0u ? 0.0 : lod;

            color.xyz += clamp(textureLod(cubeMap, L, lod).rgb, vec3(0.0), vec3(5.0)) * NdotL;
            color.w += NdotL;

        }

    }

    return color.rgb / color.a;

}