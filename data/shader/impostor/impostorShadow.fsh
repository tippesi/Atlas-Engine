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
layout(location=8) flat in vec3 instanceScale;
#endif

layout(set = 3, binding = 0) uniform sampler2DArray baseColorMap;
#ifdef PIXEL_DEPTH_OFFSET
layout(set = 3, binding = 1) uniform sampler2DArray depthMap;
#endif

layout(std430, set = 1, binding = 3) buffer Matrices {
    mat4 matrices[];
};

layout(set = 3, binding = 3, std140) uniform UniformBuffer{
	vec4 center;

    float radius;
	int views;

	float cutoff;
	float mipBias;
} uniforms;

layout(push_constant) uniform constants {
    mat4 lightSpaceMatrix;

    vec4 lightLocation;
	vec4 lightUp;
	vec4 lightRight;
} pushConstants;

void main() {

    float alpha;

#ifdef INTERPOLATION
    vec4 alpha0 = texture(baseColorMap, vec3(texCoordVS, float(index0VS)), uniforms.mipBias).a;
    vec4 alpha1 = texture(baseColorMap, vec3(texCoordVS, float(index1VS)), uniforms.mipBias).a;
    vec4 alpha2 = texture(baseColorMap, vec3(texCoordVS, float(index2VS)), uniforms.mipBias).a;

    alpha = weight0VS * alpha0 + 
		weight1VS * alpha1 + 
		weight2VS * alpha2;
#else
    alpha = texture(baseColorMap, vec3(texCoordVS, float(indexVS)), uniforms.mipBias).a;
#endif

    if (alpha < uniforms.cutoff)
        discard;

#ifdef PIXEL_DEPTH_OFFSET
#ifdef INTERPOLATION
    float depth0 = texture(depthMap, vec3(texCoordVS, float(index0VS)), uniforms.mipBias).r;
    float depth1 = texture(depthMap, vec3(texCoordVS, float(index1VS)), uniforms.mipBias).r;
    float depth2 = texture(depthMap, vec3(texCoordVS, float(index2VS)), uniforms.mipBias).r;

    float depthOffset = weight0VS * depth0 +
        weight1VS * depth1 +
        weight2VS * depth2;
#else
    float depthOffset = texture(depthMap, vec3(texCoordVS, float(indexVS)), uniforms.mipBias).r;
#endif
    vec3 modelPosition = modelPositionVS + depthOffset * normalize(pushConstants.lightLocation.xyz) * instanceScale;
    vec4 modelPositionFS = pushConstants.lightSpaceMatrix * vec4(modelPosition.xyz, 1.0);
    float modelDepth = modelPositionFS.z / modelPositionFS.w;
    gl_FragDepth = modelDepth;
#endif

}