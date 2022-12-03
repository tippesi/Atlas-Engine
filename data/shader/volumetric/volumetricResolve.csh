#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <../common/flatten.hsh>
#include <fog.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(binding = 0) uniform sampler2D lowResVolumetricTexture;
layout(binding = 1) uniform sampler2D lowResDepthTexture;
layout(binding = 2) uniform sampler2D lowResVolumetricCloudsTexture;
layout(binding = 3) uniform sampler2D depthTexture;
layout(binding = 0, rgba16f) uniform image2D resolveImage;

uniform mat4 ivMatrix;
uniform vec3 cameraLocation;
uniform bool downsampled2x;
uniform bool cloudsEnabled = true;

// (localSize / 2 + 2)^2
shared float depths[36];
shared vec3 volumetrics[36];

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
		volumetrics[gl_LocalInvocationIndex] = texelFetch(lowResVolumetricTexture, offset, 0).rgb;
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

vec4 Upsample2x(float referenceDepth) {

    ivec2 pixel = ivec2(gl_LocalInvocationID) / 2 + ivec2(1);

	float invocationDepths[9];

	for (uint i = 0; i < 9; i++) {
		int sharedMemoryOffset = Flatten2D(pixel + offsets[i], unflattenedDepthDataSize);
		invocationDepths[i] = depths[sharedMemoryOffset];
	}

    int idx = NearestDepth(referenceDepth, invocationDepths);
	int offset = Flatten2D(pixel + offsets[idx], unflattenedDepthDataSize);

    return vec4(volumetrics[offset], 1.0);

}

void main() {

    if (downsampled2x) LoadGroupSharedData();

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(resolveImage).x ||
        pixel.y > imageSize(resolveImage).y)
        return;

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(imageSize(resolveImage));

    float depth = texelFetch(depthTexture, pixel, 0).r;

    vec4 volumetric;
    if (downsampled2x) {
        volumetric = Upsample2x(depth);
    }
    else {
        volumetric = vec4(textureLod(lowResVolumetricTexture, texCoord, 0).rgb, 0.0);
    }

	vec3 viewPosition = ConvertDepthToViewSpace(depth, texCoord);
	vec3 worldPosition = vec3(ivMatrix * vec4(viewPosition, 1.0));

    vec4 resolve = imageLoad(resolveImage, pixel);	

	float fogAmount = fogEnabled ? saturate(ComputeVolumetricFog(cameraLocation, worldPosition)) : 0.0;
    resolve = fogEnabled ? mix(vec4(fogColor, 1.0), resolve, fogAmount) + volumetric : resolve + volumetric;

    if (cloudsEnabled) {
        vec4 cloudScattering = texture(lowResVolumetricCloudsTexture, texCoord);
        float alpha = cloudScattering.a;
        resolve = alpha * resolve + cloudScattering;
    }

    imageStore(resolveImage, pixel, resolve);

}