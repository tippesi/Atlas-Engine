#include <deferred.hsh>

#include <../brdf/brdfEval.hsh>
#include <../common/flatten.hsh>

#ifdef DDGI
#include <../ddgi/ddgi.hsh>
#endif

layout (local_size_x = 8, local_size_y = 8) in;

layout(set = 3, binding = 0, rgba16f) uniform image2D image;
layout(set = 3, binding = 1) uniform sampler2D aoTexture;
layout(set = 3, binding = 2) uniform sampler2D reflectionTexture;
layout(set = 3, binding = 3) uniform sampler2D giTexture;
layout(set = 3, binding = 4) uniform sampler2D lowResDepthTexture;
layout(set = 3, binding = 5) uniform sampler2D lowResNormalTexture;

layout(set = 3, binding = 6) uniform UniformBuffer {
    int aoDownsampled2x;
    int reflectionDownsampled2x;
    int giDownsampled2x;
    float aoStrength;
    int specularProbeMipLevels;
} Uniforms;

struct UpsampleResult {
    vec3 reflection;
    vec4 gi;
};

// (localSize / 2 + 2)^2
shared float depths[36];
shared vec3 normals[36];
shared float aos[36];
shared vec3 reflections[36];
shared vec4 gi[36];

const uint depthDataSize = (gl_WorkGroupSize.x / 2 + 2) * (gl_WorkGroupSize.y / 2 + 2);
const ivec2 unflattenedDepthDataSize = ivec2(gl_WorkGroupSize) / 2 + 2;

void LoadGroupSharedData() {

    ivec2 workGroupOffset = ivec2(gl_WorkGroupID) * ivec2(gl_WorkGroupSize) / 2 - ivec2(1);

    // We assume data size is smaller than gl_WorkGroupSize.x + gl_WorkGroupSize.y
    if (gl_LocalInvocationIndex < depthDataSize) {
        ivec2 offset = Unflatten2D(int(gl_LocalInvocationIndex), unflattenedDepthDataSize);
        offset += workGroupOffset;
        offset = clamp(offset, ivec2(0), textureSize(lowResDepthTexture, 0));
        depths[gl_LocalInvocationIndex] = ConvertDepthToViewSpaceDepth(texelFetch(lowResDepthTexture, offset, 0).r);

        vec3 normal = DecodeNormal(texelFetch(lowResNormalTexture, offset, 0).rg);
        normals[gl_LocalInvocationIndex] = normalize(normal);
#ifdef AO
        if (Uniforms.aoDownsampled2x > 0)
            aos[gl_LocalInvocationIndex] = texelFetch(aoTexture, offset, 0).r;
#endif
#ifdef REFLECTION
        if (Uniforms.reflectionDownsampled2x > 0)
            reflections[gl_LocalInvocationIndex] = texelFetch(reflectionTexture, offset, 0).rgb;
#endif
#if defined(SSGI) || defined(RTGI)
        if (Uniforms.giDownsampled2x > 0)
            gi[gl_LocalInvocationIndex] = texelFetch(giTexture, offset, 0);
#endif
    }

    barrier();

}

const ivec2 offsets[9] = ivec2[9](
    ivec2(-1, -1),
    ivec2(0, -1),
    ivec2(1, -1),
    ivec2(-1, 0),
    ivec2(0, 0),
    ivec2(1, 0),
    ivec2(-1, 1),
    ivec2(0, 1),
    ivec2(1, 1)
);

const ivec2 pixelOffsets[4] = ivec2[4](
    ivec2(0, 0),
    ivec2(1, 0),
    ivec2(0, 1),
    ivec2(1, 1)
);

int NearestDepth(float referenceDepth, float[9] depthVec) {

    int idx = 0;
    float nearest = distance(referenceDepth, depthVec[0]);
    for (int i = 1; i < 9; i++) {
        float dist = distance(referenceDepth, depthVec[i]);
        if (dist < nearest) {
            nearest = dist;
            idx = i;
        }
    }
    return idx;

}

float UpsampleAo2x(float referenceDepth) {

    ivec2 pixel = ivec2(gl_LocalInvocationID) / 2 + ivec2(1);

    float invocationDepths[9];

    referenceDepth = ConvertDepthToViewSpaceDepth(referenceDepth);

    for (uint i = 0; i < 9; i++) {
        int sharedMemoryOffset = Flatten2D(pixel + offsets[i], unflattenedDepthDataSize);
        invocationDepths[i] = depths[sharedMemoryOffset];
    }

    int idx = NearestDepth(referenceDepth, invocationDepths);
    int offset = Flatten2D(pixel + offsets[idx], unflattenedDepthDataSize);

    return aos[offset];

}

float GetPixelEdgeWeight(int sharedMemoryOffset, float referenceDepth, vec3 referenceNormal) {
    float depth = depths[sharedMemoryOffset];

    float depthDiff = abs(referenceDepth - depth);
    float depthWeight = min(exp(-depthDiff * 4.0), 1.0);

    float normalWeight = min(pow(max(dot(referenceNormal, normals[sharedMemoryOffset]), 0.0), 128.0), 1.0);

    return depthWeight * normalWeight;
}

UpsampleResult Upsample(float referenceDepth, vec3 referenceNormal, vec2 highResPixel) {

    UpsampleResult result;

    highResPixel /= 2.0;
    float x = fract(highResPixel.x);
    float y = fract(highResPixel.y);

    float weights[4] = { (1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y };

    ivec2 pixel = ivec2(gl_LocalInvocationID) / 2 + ivec2(1);

    referenceDepth = ConvertDepthToViewSpaceDepth(referenceDepth);

    result.gi = vec4(0.0);
    result.reflection = vec3(0.0);

    float totalWeight = 0.0;

    for (uint i = 0; i < 4; i++) {
        int sharedMemoryOffset = Flatten2D(pixel + pixelOffsets[i], unflattenedDepthDataSize);
        
        float edgeWeight = GetPixelEdgeWeight(sharedMemoryOffset, referenceDepth, referenceNormal);
        float weight = edgeWeight * weights[i];

#if defined(SSGI) || defined(RTGI)
        result.gi += gi[sharedMemoryOffset] * weight;
#endif
#ifdef REFLECTION
        result.reflection += reflections[sharedMemoryOffset] * weight;
#endif

        totalWeight += weight;
    }

    result.reflection /= totalWeight;
    result.gi /= totalWeight;

    float maxWeight = 0.0;
    int maxMemoryIdx = 0;
    for (uint i = 0; i < 9; i++) {
        int sharedMemoryOffset = Flatten2D(pixel + offsets[i], unflattenedDepthDataSize);
        
        float edgeWeight = GetPixelEdgeWeight(sharedMemoryOffset, referenceDepth, referenceNormal);
        if (edgeWeight > maxWeight) {
            maxWeight = edgeWeight;
            maxMemoryIdx = sharedMemoryOffset;
        }
    }

    if (totalWeight < 10e-3) {
        result.gi = gi[maxMemoryIdx];
        result.reflection = reflections[maxMemoryIdx];
    }

    result.gi = max(result.gi, vec4(0.0));
    result.reflection = max(result.reflection, vec3(0.0));

    return result;

}

void main() {

    if (Uniforms.aoDownsampled2x > 0 || Uniforms.giDownsampled2x > 0 || Uniforms.reflectionDownsampled2x > 0) LoadGroupSharedData();

    if (gl_GlobalInvocationID.x > imageSize(image).x ||
        gl_GlobalInvocationID.y > imageSize(image).y)
        return;

    ivec2 resolution = imageSize(image);
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(resolution);

    float depth = texelFetch(depthTexture, pixel, 0).r;
    vec3 indirect = vec3(0.0);

    UpsampleResult upsampleResult;

    if (depth < 1.0) {

        vec3 geometryNormal;
        // We don't have any light direction, that's why we use vec3(0.0, -1.0, 0.0) as a placeholder
        Surface surface = GetSurface(texCoord, depth, vec3(0.0, -1.0, 0.0), geometryNormal);

        vec3 worldView = normalize(vec3(globalData.ivMatrix * vec4(surface.P, 0.0)));
        vec3 worldPosition = vec3(globalData.ivMatrix * vec4(surface.P, 1.0));
        vec3 worldNormal = normalize(vec3(globalData.ivMatrix * vec4(surface.N, 0.0)));
        vec3 geometryWorldNormal = normalize(vec3(globalData.ivMatrix * vec4(geometryNormal, 0.0)));

        upsampleResult = Upsample(depth, surface.N, vec2(pixel));

        // Indirect diffuse BRDF
#ifdef RTGI
        vec3 rtgi = Uniforms.giDownsampled2x > 0 ? upsampleResult.gi.rgb : textureLod(giTexture, texCoord, 0.0).rgb;
        vec3 indirectDiffuse = rtgi * EvaluateIndirectDiffuseBRDF(surface);
#else
#ifdef DDGI
        vec3 prefilteredDiffuse = textureLod(diffuseProbe, worldNormal, 0).rgb;
        vec4 prefilteredDiffuseLocal = ddgiData.volumeEnabled > 0 ?
            GetLocalIrradianceInterpolated(worldPosition, worldView, worldNormal, geometryWorldNormal, prefilteredDiffuse) : vec4(0.0, 0.0, 0.0, 1.0);
        prefilteredDiffuseLocal = IsInsideVolume(worldPosition) ? prefilteredDiffuseLocal : vec4(0.0, 0.0, 0.0, 1.0);
        prefilteredDiffuse = prefilteredDiffuseLocal.rgb + prefilteredDiffuse * prefilteredDiffuseLocal.a;
        vec3 indirectDiffuse = prefilteredDiffuse * EvaluateIndirectDiffuseBRDF(surface) * ddgiData.volumeStrength;
#else
        vec3 prefilteredDiffuse = textureLod(diffuseProbe, worldNormal, 0).rgb;
        vec3 indirectDiffuse = prefilteredDiffuse * EvaluateIndirectDiffuseBRDF(surface);
#endif
#endif

        // Indirect specular BRDF
        vec3 R = normalize(mat3(globalData.ivMatrix) * reflect(-surface.V, surface.N));
        float mipLevel = surface.material.roughness * float(Uniforms.specularProbeMipLevels - 1);
        vec3 prefilteredSpecular = textureLod(specularProbe, R, mipLevel).rgb;
        // We multiply by local sky visibility because the reflection probe only includes the sky
        //vec3 indirectSpecular = prefilteredSpecular * EvaluateIndirectSpecularBRDF(surface)
        //    * prefilteredDiffuseLocal.a;        

#ifdef REFLECTION
        vec3 indirectSpecular = Uniforms.reflectionDownsampled2x > 0 ? upsampleResult.reflection : textureLod(reflectionTexture, texCoord, 0.0).rgb;
#else
#ifdef DDGI
        vec3 indirectSpecular = IsInsideVolume(worldPosition) ? vec3(0.0) : prefilteredSpecular;
#else
        vec3 indirectSpecular = prefilteredSpecular;
#endif
#endif

        indirectSpecular *= EvaluateIndirectSpecularBRDF(surface);
        indirect = (indirectDiffuse + indirectSpecular) * surface.material.ao;

#ifdef SSGI
        vec4 ssgi = Uniforms.giDownsampled2x > 0 ? upsampleResult.gi : textureLod(giTexture, texCoord, 0.0);
#endif

        // This normally only accounts for diffuse occlusion, we need seperate terms
        // for diffuse and specular.
#ifdef AO
        float occlusionFactor = Uniforms.aoDownsampled2x > 0 ? UpsampleAo2x(depth) : textureLod(aoTexture, texCoord, 0.0).r;

        indirect *= vec3(pow(occlusionFactor, Uniforms.aoStrength));
#endif
#ifdef SSGI
        // Only apply SSGI ao if normal AO is turned off
#ifndef AO
        indirect *= vec3(pow(ssgi.a, Uniforms.aoStrength));
#endif
        indirect += EvaluateIndirectDiffuseBRDF(surface) * ssgi.rgb;
#endif

    }

    vec3 direct = imageLoad(image, pixel).rgb;
    imageStore(image, pixel, vec4(vec3(direct + indirect), 0.0));
    //imageStore(image, pixel, vec4(vec3(pow(textureLod(giTexture, texCoord, 0.0).a, Uniforms.aoStrength)), 0.0));
    //imageStore(image, pixel, vec4(vec3(pow(upsampleResult.gi.a, Uniforms.aoStrength)), 0.0));

}