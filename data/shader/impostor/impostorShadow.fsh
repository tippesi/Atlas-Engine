#include <../globals.hsh>

layout(location = 0) in vec2 texCoordVS;
#ifdef INTERPOLATION
layout(location=1) flat in int index0VS;
layout(location=2) flat in int index1VS;
layout(location=3) flat in int index2VS;

layout(location=4) flat in float weight0VS;
layout(location=5) flat in float weight1VS;
layout(location=6) flat in float weight2VS;
#else
layout(location=1) flat in int indexVS;
#endif
#ifdef PIXEL_DEPTH_OFFSET
layout(location=7) in vec3 modelPositionVS;
layout(location=8) flat in mat4 instanceMatrix;
#endif

layout(set = 3, binding = 0) uniform sampler2DArray baseColorMap;
#ifdef PIXEL_DEPTH_OFFSET
layout(set = 3, binding = 2) uniform sampler2DArray depthMap;
#endif

layout(std430, set = 1, binding = 2) buffer Matrices {
    mat4 matrices[];
};

layout(push_constant) uniform constants {
    mat4 lightSpaceMatrix;

    vec4 lightLocation;
    vec4 center;

    float radius;
    int views;
    float cutoff;
    float mipBias;
} PushConstants;

void main() {

    vec4 baseColor;

#ifdef INTERPOLATION
    vec4 baseColor0 = texture(baseColorMap, vec3(texCoordVS, float(index0VS)), PushConstants.mipBias).rgba;
    vec4 baseColor1 = texture(baseColorMap, vec3(texCoordVS, float(index1VS)), PushConstants.mipBias).rgba;
    vec4 baseColor2 = texture(baseColorMap, vec3(texCoordVS, float(index2VS)), PushConstants.mipBias).rgba;

    baseColor = weight0VS * baseColor0 + 
		weight1VS * baseColor1 + 
		  weight2VS * baseColor2;
#else
    baseColor = texture(baseColorMap, vec3(texCoordVS, float(indexVS)), PushConstants.mipBias).rgba;
#endif

    if (baseColor.a < PushConstants.cutoff)
        discard;

#ifdef PIXEL_DEPTH_OFFSET
#ifdef INTERPOLATION
    float depth0 = texture(depthMap, vec3(texCoordVS, float(index0VS)), PushConstants.mipBias).r;
    float depth1 = texture(depthMap, vec3(texCoordVS, float(index1VS)), PushConstants.mipBias).r;
    float depth2 = texture(depthMap, vec3(texCoordVS, float(index2VS)), PushConstants.mipBias).r;

    float depthOffset = weight0VS * depth0 +
        weight1VS * depth1 +
        weight2VS * depth2;
#else
    float depthOffset = texture(depthMap, vec3(texCoordVS, float(indexVS)), PushConstants.mipBias).r;
#endif
    vec3 modelPosition = modelPositionVS + depthOffset * normalize(PushConstants.lightLocation.xyz);
    vec4 modelPositionFS = instanceMatrix * vec4(modelPosition.xyz, 1.0);
    float modelDepth = modelPositionFS.z / modelPositionFS.w;
    gl_FragDepth = modelDepth;
#endif

}