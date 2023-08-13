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
    ivec2 filteredSize;
} pushConstants;

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

#ifdef FILTER_DIFFUSE
    uint sampleCount = 256u;
#else
    uint sampleCount = 64u;
#endif
    vec4 acc = vec4(0.0);

    ivec2 size = textureSize(cubeMap, 0);
    vec3 N = normalize(worldDirection);

    // This is a necessary approximation for the specular probe
    vec3 V = N;

    float totalWeight = 0.0;

    for (uint i = 0u; i < sampleCount; i++) {
        float raySeed = float(pixel.x + pixel.y * size.x) + pushConstants.roughness;
        float curSeed = float(i);
        float u0 = random(raySeed, curSeed);
        curSeed += 0.5;
        float u1 = random(raySeed, curSeed);

        vec2 Xi = vec2(u0, u1);

        vec3 L;
        float NdotL;
        float pdf;

        float alpha = sqr(pushConstants.roughness);

#ifdef FILTER_DIFFUSE
        ImportanceSampleCosDir(N, Xi, L, NdotL, pdf);
#else
        Xi = Hammersley(i, sampleCount);

        ImportanceSampleGGX(Xi, N, V, alpha, L, pdf);

        NdotL = dot(N, L);
        vec3 H = normalize(L + V);
#endif

        NdotL = saturate(NdotL);
        if (NdotL > 0.0) {
#ifdef FILTER_DIFFUSE
            acc.xyz += min(textureLod(cubeMap, L, 5.0).rgb, 1.0);
            totalWeight += 1.0;
#else
            float NdotH = saturate(dot(N , H));
            float LdotH = saturate(dot(L , H));

            pdf = DistributionGGX(NdotH, alpha) / 4.0;

            float solidAngleTexel = 4.0 * PI / (6.0 * float(size.x * size.y));
            float solidAngleSample = 1.0 / (float(sampleCount) * pdf);
            float lod = clamp(0.5 * log2(float(solidAngleSample / solidAngleTexel)) + 1.0, 
                0.0, float(pushConstants.cubeMapMipLevels - 1.0));

            acc.xyz += clamp(textureLod(cubeMap, L, lod).rgb, vec3(0.0), vec3(5.0)) * NdotL;
            totalWeight += NdotL;
#endif
        }

    }

    vec4 color = acc / totalWeight;

    imageStore(filteredCubeMap, ivec3(gl_GlobalInvocationID), color);

}