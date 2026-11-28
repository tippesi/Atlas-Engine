#extension GL_EXT_nonuniform_qualifier : require

#define SHADOW_FILTER_VOGEL
//#define SHADOW_CASCADE_BLENDING

layout (local_size_x = 16, local_size_y = 16) in;

#include <deferred.hsh>
#include <lightCulling.hsh>

#include <../structures.hsh>
#include <../shadow.hsh>
#include <../globals.hsh>

#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../common/light.hsh>
#include <../brdf/brdfEval.hsh>
#include <../common/octahedron.hsh>
#include <../clouds/shadow.hsh>

layout(set = 3, binding = 0, rgba16f) uniform image2D image;

#ifdef SCREEN_SPACE_SHADOWS
layout(set = 3, binding = 1) uniform sampler2D sssTexture;
#endif
#ifdef CLOUD_SHADOWS
layout(set = 3, binding = 2) uniform sampler2D cloudMap;
#endif

layout(set = 3, binding = 4) uniform sampler shadowSampler;

#include <directLight.hsh>

layout(std430, set = 3, binding = 6) buffer LightBucketsBuffer {
    int lightBuckets[];
};

layout(push_constant) uniform constants {
    int lightCount;
    int lightBucketCount;
    int padding1;
    int padding2;
} pushConstants;

shared uint sharedLightBuckets[lightBucketCount];

void LoadGroupSharedData() {

    int lightBucketOffset = GetLightBucketsGroupOffset();
    int groupSize = int(gl_WorkGroupSize.x * gl_WorkGroupSize.y);

    int localOffset = int(gl_LocalInvocationIndex);
    for (int i = localOffset; i < pushConstants.lightBucketCount; i += groupSize) {
        sharedLightBuckets[i] = uint(lightBuckets[lightBucketOffset + i]);
    }

    barrier();

}

void main() {

    LoadGroupSharedData();

    ivec2 resolution = imageSize(image);
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(resolution);

    float depth = texelFetch(depthTexture, pixel, 0).r;

    int lightCount = 0;

    // Load to get the atmosphere which was rendered before
    vec3 direct = imageLoad(image, pixel).rgb;

    if (depth < 1.0) {
        vec3 geometryNormal;
        // We don't have any light direction, that's why we use vec3(0.0, -1.0, 0.0) as a placeholder
        Surface surface = GetSurface(texCoord, depth, vec3(0.0, -1.0, 0.0), geometryNormal);

        direct = vec3(0.0);        

        for (uint i = 0u; i < pushConstants.lightBucketCount; i++) {
            uint lightBucket = sharedLightBuckets[i];
            if (lightBucket == 0) continue;

            // Get most significant bit index
            uint bucketBitCount = findMSB(lightBucket) + 1u;
            uint offsetIdx = i * 32u;

            for (uint j = 0u; j < bucketBitCount; j++) {

                bool lightVisible = (lightBucket & (1 << j)) > 0u;
                if (!lightVisible) continue;

                lightCount++;

                Light light = lights[offsetIdx + j];

                bool isMain = i + j == 0u ? true : false;
                direct += EvaluateLight(light, surface, geometryNormal, isMain, pixel, texCoord);
            }
        }


        if (dot(surface.material.emissiveColor, vec3(1.0)) > 0.01) {    
            direct += surface.material.emissiveColor * globalData.preExposure;
        }
    }

    //direct = vec3(float(lightCount) / 64.0);
    imageStore(image, pixel, vec4(direct, 1.0));

}
