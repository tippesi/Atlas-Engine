layout (local_size_x = 8, local_size_y = 8) in;

#include <../globals.hsh>
#include <../common/convert.hsh>
#include <../clouds/shadow.hsh>

#include <fog.hsh>
#include <froxel.hsh>

layout(set = 3, binding = 0, rgba16f) writeonly uniform image3D integratedFroxelImage;
layout(set = 3, binding = 1) uniform sampler3D froxelTexture;
layout(set = 3, binding = 5) uniform sampler2D volumetricCloudTexture;

layout(std140, set = 3, binding = 7) uniform UniformBuffer {
    int sampleCount;
    float intensity;
    int fogEnabled;
    float oceanHeight;
    int lightCount;
    int offsetX;
    int offsetY;
    int directionalLightCount;
    vec4 planetCenterAndRadius;
    Fog fog;
    CloudShadow cloudShadow;
} uniforms;

void main() {

    ivec2 column = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(column, pushConstants.columnCount)))
        return;

    vec2 volumetricResolution = vec2(pushConstants.resolution);
    vec2 texCoord = GetFroxelColumnTexCoord(column, volumetricResolution);

    vec3 viewDirection = normalize(ConvertDepthToViewSpace(1.0, texCoord));
    float distancePerDepth = 1.0 / max(-viewDirection.z, 1e-3);
    vec3 worldDirection = vec3(globalData.ivMatrix * vec4(viewDirection, 0.0));

    vec3 foginess = vec3(0.0);
    vec4 extinction = vec4(1.0);
    vec4 extinctionWithClouds = vec4(1.0);

#ifdef CLOUDS
    bool receivedCloudExtinction = false;
    float cloudExtinction = min(textureLod(volumetricCloudTexture, texCoord, 0.0).a, 1.0);
    vec3 planetCenter = uniforms.planetCenterAndRadius.xyz;
    float cloudInnerRadius = uniforms.planetCenterAndRadius.w;
#endif

    float sliceStart = 0.0;
    for (int i = 0; i < pushConstants.sliceCount; i++) {
        float sliceEnd = GetFroxelSliceDepth(float(i + 1));
        float stepLength = (sliceEnd - sliceStart) * distancePerDepth;

        vec4 froxel = texelFetch(froxelTexture, ivec3(column, i), 0);
        float density = froxel.a;

        vec3 scatteringCoefficient = uniforms.fog.scatteringFactor *
            uniforms.fog.extinctionCoefficients.rgb * density;
        vec4 extinctionCoefficient = uniforms.fog.extinctionFactor *
            uniforms.fog.extinctionCoefficients * density;

        vec4 clampedExtinction = max(extinctionCoefficient, 0.0000001);
        vec4 stepExtinction = exp(-extinctionCoefficient * stepLength);

        vec3 stepScattering = scatteringCoefficient * froxel.rgb;
        vec3 luminanceIntegral = (stepScattering - stepScattering * stepExtinction.rgb) / clampedExtinction.rgb;

#ifdef CLOUDS
        foginess += luminanceIntegral * extinctionWithClouds.rgb;

        vec3 worldPosition = globalData.cameraLocation.xyz +
            worldDirection * (0.5 * (sliceStart + sliceEnd) * distancePerDepth);
        float distToPlanetCenter = distance(worldPosition, planetCenter);
        if (distToPlanetCenter > cloudInnerRadius && !receivedCloudExtinction) {
            extinctionWithClouds *= uniforms.fogEnabled > 0 ? stepExtinction * cloudExtinction : vec4(1.0);
            receivedCloudExtinction = true;
        }
        else {
            extinctionWithClouds *= uniforms.fogEnabled > 0 ? stepExtinction : vec4(1.0);
        }
#else
        foginess += luminanceIntegral * extinction.rgb;
#endif

        extinction *= uniforms.fogEnabled > 0 ? stepExtinction : vec4(1.0);

        imageStore(integratedFroxelImage, ivec3(column, i), vec4(foginess * uniforms.intensity, extinction.a));

        sliceStart = sliceEnd;
    }

}
