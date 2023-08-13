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
layout(set = 3, binding = 3) uniform sampler2D lowResDepthTexture;

layout(set = 3, binding = 4) uniform UniformBuffer {
    int aoEnabled;
    int aoDownsampled2x;
    int reflectionEnabled;
    float aoStrength;
    int specularProbeMipLevels;
} Uniforms;

// (localSize / 2 + 2)^2
shared float depths[36];
shared float aos[36];
shared vec3 reflections[36];

const uint depthDataSize = (gl_WorkGroupSize.x / 2 + 2) * (gl_WorkGroupSize.y / 2 + 2);
const ivec2 unflattenedDepthDataSize = ivec2(gl_WorkGroupSize) / 2 + 2;

void LoadGroupSharedData() {

    ivec2 workGroupOffset = ivec2(gl_WorkGroupID) * ivec2(gl_WorkGroupSize) / 2 - ivec2(1);

    // We assume data size is smaller than gl_WorkGroupSize.x + gl_WorkGroupSize.y
    if (gl_LocalInvocationIndex < depthDataSize) {
        ivec2 offset = Unflatten2D(int(gl_LocalInvocationIndex), unflattenedDepthDataSize);
        offset += workGroupOffset;
        offset = clamp(offset, ivec2(0), textureSize(lowResDepthTexture, 0));
        depths[gl_LocalInvocationIndex] = texelFetch(lowResDepthTexture, offset, 0).r;
        aos[gl_LocalInvocationIndex] = texelFetch(aoTexture, offset, 0).r;
        reflections[gl_LocalInvocationIndex] = texelFetch(reflectionTexture, offset, 0).rgb;
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

    for (uint i = 0; i < 9; i++) {
        int sharedMemoryOffset = Flatten2D(pixel + offsets[i], unflattenedDepthDataSize);
        invocationDepths[i] = depths[sharedMemoryOffset];
    }

    int idx = NearestDepth(referenceDepth, invocationDepths);
    int offset = Flatten2D(pixel + offsets[idx], unflattenedDepthDataSize);

    return aos[offset];

}

vec3 UpsampleReflection2x(float referenceDepth, vec2 texCoords) {

    ivec2 pixel = ivec2(gl_LocalInvocationID) / 2 + ivec2(1);

    float invocationDepths[9];

    float minWeight = 1.0;

    for (uint i = 0; i < 9; i++) {
        int sharedMemoryOffset = Flatten2D(pixel + offsets[i], unflattenedDepthDataSize);
        float depth = depths[sharedMemoryOffset];

        float depthDiff = abs(referenceDepth - depth);
        float depthWeight = min(exp(-depthDiff * 32.0), 1.0);
        minWeight = min(minWeight, depthWeight);

        invocationDepths[i] = depth;
    }

    int idx = NearestDepth(referenceDepth, invocationDepths);
    int offset = Flatten2D(pixel + offsets[idx], unflattenedDepthDataSize);

    vec3 bilinearReflection = textureLod(reflectionTexture, texCoords, 0).rgb;
    return mix(reflections[offset], bilinearReflection, minWeight);

}

void main() {

    if (Uniforms.aoDownsampled2x > 0) LoadGroupSharedData();

    if (gl_GlobalInvocationID.x > imageSize(image).x ||
        gl_GlobalInvocationID.y > imageSize(image).y)
        return;

    ivec2 resolution = imageSize(image);
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(resolution);

    float depth = texelFetch(depthTexture, pixel, 0).r;
    vec3 indirect = vec3(0.0);

    if (depth < 1.0) {

        vec3 geometryNormal;
        // We don't have any light direction, that's why we use vec3(0.0, -1.0, 0.0) as a placeholder
        Surface surface = GetSurface(texCoord, depth, vec3(0.0, -1.0, 0.0), geometryNormal);

        vec3 worldView = normalize(vec3(globalData.ivMatrix * vec4(surface.P, 0.0)));
        vec3 worldPosition = vec3(globalData.ivMatrix * vec4(surface.P, 1.0));
        vec3 worldNormal = normalize(vec3(globalData.ivMatrix * vec4(surface.N, 0.0)));
        vec3 geometryWorldNormal = normalize(vec3(globalData.ivMatrix * vec4(geometryNormal, 0.0)));

        // Indirect diffuse BRDF
#ifdef DDGI
        vec3 prefilteredDiffuse = textureLod(diffuseProbe, worldNormal, 0).rgb;
        vec4 prefilteredDiffuseLocal = ddgiData.volumeEnabled > 0 ?
            GetLocalIrradiance(worldPosition, worldView, worldNormal, geometryWorldNormal) : vec4(0.0, 0.0, 0.0, 1.0);
        prefilteredDiffuseLocal = IsInsideVolume(worldPosition) ? prefilteredDiffuseLocal : vec4(0.0, 0.0, 0.0, 1.0);
        prefilteredDiffuse = prefilteredDiffuseLocal.rgb + prefilteredDiffuse * prefilteredDiffuseLocal.a;
        vec3 indirectDiffuse = prefilteredDiffuse * EvaluateIndirectDiffuseBRDF(surface) * ddgiData.volumeStrength;
#else
        vec3 prefilteredDiffuse = textureLod(diffuseProbe, worldNormal, 0).rgb;
        vec3 indirectDiffuse = prefilteredDiffuse * EvaluateIndirectDiffuseBRDF(surface);
#endif

        // Indirect specular BRDF
        vec3 R = normalize(mat3(globalData.ivMatrix) * reflect(-surface.V, surface.N));
        float mipLevel = surface.material.roughness * float(Uniforms.specularProbeMipLevels - 1);
        vec3 prefilteredSpecular = textureLod(specularProbe, R, mipLevel).rgb;
        // We multiply by local sky visibility because the reflection probe only includes the sky
        //vec3 indirectSpecular = prefilteredSpecular * EvaluateIndirectSpecularBRDF(surface)
        //    * prefilteredDiffuseLocal.a;
#ifdef REFLECTION
        vec3 indirectSpecular = Uniforms.reflectionEnabled > 0 ? true ?
            UpsampleReflection2x(depth, texCoord) : texture(reflectionTexture, texCoord).rgb : vec3(0.0);
#else
#ifdef DDGI
        vec3 indirectSpecular = IsInsideVolume(worldPosition) ? vec3(0.0) : prefilteredSpecular;
#else
        vec3 indirectSpecular = prefilteredSpecular;
#endif
#endif

        indirectSpecular *= EvaluateIndirectSpecularBRDF(surface);
        indirect = (indirectDiffuse + indirectSpecular) * surface.material.ao;

        // This normally only accounts for diffuse occlusion, we need seperate terms
        // for diffuse and specular.
#ifdef AO
        float occlusionFactor = Uniforms.aoEnabled > 0 ? Uniforms.aoDownsampled2x > 0 ?
            UpsampleAo2x(depth) : texture(aoTexture, texCoord).r : 1.0;
        indirect *= pow(occlusionFactor, Uniforms.aoStrength);
#endif

    }

    vec3 direct = imageLoad(image, pixel).rgb;
    imageStore(image, pixel, vec4(direct + indirect, 0.0));

}