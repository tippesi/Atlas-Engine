#include <deferred.hsh>

#include <../ddgi/ddgi.hsh>
#include <../brdf/brdfEval.hsh>
#include <../common/flatten.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(binding = 0, rgba16f) uniform image2D image;
layout(binding = 6) uniform sampler2D aoTexture;
layout(binding = 16) uniform sampler2D reflectionTexture;
layout(binding = 14) uniform sampler2D lowResDepthTexture;

uniform mat4 ivMatrix;

uniform float indirectStrength = 1.0;
uniform bool aoEnabled = true;
uniform bool aoDownsampled2x;
uniform bool reflectionEnabled = true;
uniform float aoStrength = 1.0;

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

vec3 UpsampleReflection2x(float referenceDepth) {

    ivec2 pixel = ivec2(gl_LocalInvocationID) / 2 + ivec2(1);

	float invocationDepths[9];

	for (uint i = 0; i < 9; i++) {
		int sharedMemoryOffset = Flatten2D(pixel + offsets[i], unflattenedDepthDataSize);
		invocationDepths[i] = depths[sharedMemoryOffset];
	}

    int idx = NearestDepth(referenceDepth, invocationDepths);
	int offset = Flatten2D(pixel + offsets[idx], unflattenedDepthDataSize);

    return reflections[offset];

}

void main() {

	if (aoDownsampled2x) LoadGroupSharedData();

	if (gl_GlobalInvocationID.x > imageSize(image).x ||
        gl_GlobalInvocationID.y > imageSize(image).y)
        return;

	ivec2 resolution = imageSize(image);
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(resolution);

    float depth = texelFetch(depthTexture, pixel, 0).r;

    vec3 geometryNormal;
	// We don't have any light direction, that's why we use vec3(0.0, -1.0, 0.0) as a placeholder
    Surface surface = GetSurface(texCoord, depth, vec3(0.0, -1.0, 0.0), geometryNormal);

    vec3 worldView = normalize(vec3(ivMatrix * vec4(surface.P, 0.0)));
	vec3 worldPosition = vec3(ivMatrix * vec4(surface.P, 1.0));
	vec3 worldNormal = normalize(vec3(ivMatrix * vec4(surface.N, 0.0)));
	vec3 geometryWorldNormal = normalize(vec3(ivMatrix * vec4(geometryNormal, 0.0)));

	// Indirect diffuse BRDF
	vec3 prefilteredDiffuse = textureLod(diffuseProbe, worldNormal, 0).rgb;
	vec4 prefilteredDiffuseLocal = volumeEnabled ? GetLocalIrradiance(worldPosition, worldView, worldNormal, geometryWorldNormal)
		 : vec4(0.0, 0.0, 0.0, 1.0);
	prefilteredDiffuseLocal = IsInsideVolume(worldPosition) ? prefilteredDiffuseLocal : vec4(0.0, 0.0, 0.0, 1.0);
	prefilteredDiffuse = prefilteredDiffuseLocal.rgb + prefilteredDiffuse * prefilteredDiffuseLocal.a;
	vec3 indirectDiffuse = prefilteredDiffuse * EvaluateIndirectDiffuseBRDF(surface);

	// Indirect specular BRDF
	vec3 R = normalize(mat3(ivMatrix) * reflect(-surface.V, surface.N));
	float mipLevel = sqrt(surface.material.roughness) * 9.0;
	vec3 prefilteredSpecular = textureLod(specularProbe, R, mipLevel).rgb;
	// We multiply by local sky visibility because the reflection probe only includes the sky
	//vec3 indirectSpecular = prefilteredSpecular * EvaluateIndirectSpecularBRDF(surface)
	//	* prefilteredDiffuseLocal.a;
	vec3 indirectSpecular = reflectionEnabled ? true ? UpsampleReflection2x(depth) : texture(reflectionTexture, texCoord).rgb 
		: vec3(0.0);

	indirectSpecular *= EvaluateIndirectSpecularBRDF(surface);
	vec3 indirect = (indirectDiffuse + indirectSpecular) * surface.material.ao * indirectStrength;
	
	// This normally only accounts for diffuse occlusion, we need seperate terms
	// for diffuse and specular.
	float occlusionFactor = aoEnabled ? aoDownsampled2x ? UpsampleAo2x(depth) : texture(aoTexture, texCoord).r : 1.0;
	indirect *= pow(occlusionFactor, aoStrength);

    vec3 direct = imageLoad(image, pixel).rgb;
    imageStore(image, pixel, vec4(direct + indirect, 0.0));

}