#include <../globals.hsh>

layout (location = 0) out vec4 baseColorFS;
layout (location = 1) out vec3 normalFS;
layout (location = 2) out vec3 geometryNormalFS;
layout (location = 3) out vec3 roughnessMetalnessAoFS;
layout (location = 4) out uint materialIdxFS;
layout (location = 5) out vec2 velocityFS;

layout(location=0) in vec3 positionVS;
layout(location=1) in vec2 texCoordVS;
layout(location=2) in vec3 ndcCurrentVS;
layout(location=3) in vec3 ndcLastVS;

#ifdef INTERPOLATION
layout(location=4) flat in int index0VS;
layout(location=5) flat in int index1VS;
layout(location=6) flat in int index2VS;

layout(location=7) flat in float weight0VS;
layout(location=8) flat in float weight1VS;
layout(location=9) flat in float weight2VS;
#else
layout(location=4) flat in int indexVS;
#endif

#ifdef PIXEL_DEPTH_OFFSET
layout(location=10) flat in int instanceIndexVS;
layout(location=11) in vec4 modelPositionVS;
#endif

layout(set = 3, binding = 0) uniform sampler2DArray baseColorMap;
layout(set = 3, binding = 1) uniform sampler2DArray roughnessMetalnessAoMap;
layout(set = 3, binding = 2) uniform sampler2DArray normalMap;
layout(set = 3, binding = 3) uniform sampler2DArray depthMap;

layout(std430, set = 1, binding = 2) buffer Matrices {
    mat4 matrices[];
};

layout(push_constant) uniform constants {
    vec4 center;

    float radius;
    int views;
    float cutoff;
    uint materialIdx;

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

    baseColorFS = vec4(baseColor.rgb, 1.0);

#ifdef INTERPOLATION
    vec3 normal0 = 2.0 * texture(normalMap, vec3(texCoordVS, float(index0VS)), PushConstants.mipBias).rgb - 1.0;
    vec3 normal1 = 2.0 * texture(normalMap, vec3(texCoordVS, float(index1VS)), PushConstants.mipBias).rgb - 1.0;
    vec3 normal2 = 2.0 * texture(normalMap, vec3(texCoordVS, float(index2VS)), PushConstants.mipBias).rgb - 1.0;

    geometryNormalFS = weight0VS * normal0 +
		weight1VS * normal1 + 
		weight2VS * normal2;
#else
	geometryNormalFS = 2.0 * texture(normalMap, vec3(texCoordVS, float(indexVS)), PushConstants.mipBias).rgb - 1.0;
#endif

    geometryNormalFS = normalize(vec3(globalData.vMatrix * vec4(geometryNormalFS, 0.0)));
    // We want the normal always two face the camera for two sided materials
    geometryNormalFS *= -dot(geometryNormalFS, positionVS);
    geometryNormalFS = 0.5 * geometryNormalFS + 0.5;

    normalFS = vec3(0.0);

#ifdef INTERPOLATION
    vec3 matInfo0 = texture(roughnessMetalnessAoMap, vec3(texCoordVS, float(index0VS)), PushConstants.mipBias).rgb;
    vec3 matInfo1 = texture(roughnessMetalnessAoMap, vec3(texCoordVS, float(index1VS)), PushConstants.mipBias).rgb;
    vec3 matInfo2 = texture(roughnessMetalnessAoMap, vec3(texCoordVS, float(index2VS)), PushConstants.mipBias).rgb;

    vec3 matInfo = weight0VS * matInfo0 + 
		weight1VS * matInfo1 + 
		weight2VS * matInfo2;

    roughnessMetalnessAoFS = matInfo;
#else
	roughnessMetalnessAoFS = texture(roughnessMetalnessAoMap, vec3(texCoordVS, float(indexVS)), PushConstants.mipBias).rgb;
#endif

    // Calculate velocity
	vec2 ndcL = ndcLastVS.xy / ndcLastVS.z;
	vec2 ndcC = ndcCurrentVS.xy / ndcCurrentVS.z;

	ndcL -= globalData.jitterLast;
	ndcC -= globalData.jitterCurrent;

	velocityFS = (ndcL - ndcC) * 0.5;

    materialIdxFS = PushConstants.materialIdx;

#ifdef PIXEL_DEPTH_OFFSET
    mat4 mMatrix = matrices[instanceIndexVS];
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
    vec4 modelPosition = modelPositionVS + vec4(0.0, 0.0, depthOffset, 0.0);
    vec4 modelPositionFS = globalData.pMatrix * globalData.vMatrix * mMatrix * vec4(modelPosition.xyz, 1.0);
    float modelDepth = modelPositionFS.z / modelPositionFS.w;
    gl_FragDepth = modelDepth;
#endif

}