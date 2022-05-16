#include <../common/convert.hsh>
#include <../common/utility.hsh>
#include <fog.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(binding = 0) uniform sampler2D lowResVolumetricTexture;
layout(binding = 1) uniform sampler2D lowResDepthTexture;
layout(binding = 2) uniform sampler2D depthTexture;
layout(binding = 0, rgba16f) uniform image2D resolveImage;

uniform mat4 ivMatrix;
uniform vec3 cameraLocation;
uniform bool downsampled2x;

int NearestDepth(float referenceDepth, float[4] depthVec) {

    int idx = 0;
    float nearest = distance(referenceDepth, depthVec[0]);
	for (int i = 1; i < 4; i++) {
        float dist = distance(referenceDepth, depthVec[i]);
        if (dist < nearest) {
            nearest = dist;
            idx = i;
        }
    }
	return idx;

}

// We could load the data cooperatively into shared memory
vec4 Upsample2x(float referenceDepth, ivec2 pixel, vec2 texCoord) {

    pixel /= 2;

    float depths[] = float[] (
        texelFetch(lowResDepthTexture, pixel + ivec2(0, 0), 0).r,
	    texelFetch(lowResDepthTexture, pixel + ivec2(1, 0), 0).r,
	    texelFetch(lowResDepthTexture, pixel + ivec2(0, 1), 0).r,
	    texelFetch(lowResDepthTexture, pixel + ivec2(1, 1), 0).r
    );

    vec3 volumetric[] = vec3[] (
        texelFetch(lowResVolumetricTexture, pixel + ivec2(0, 0), 0).rgb,
	    texelFetch(lowResVolumetricTexture, pixel + ivec2(1, 0), 0).rgb,
	    texelFetch(lowResVolumetricTexture, pixel + ivec2(0, 1), 0).rgb,
	    texelFetch(lowResVolumetricTexture, pixel + ivec2(1, 1), 0).rgb
    );

    int idx = NearestDepth(referenceDepth, depths);

    return vec4(volumetric[idx], 1.0);

}

void main() {

    ivec2 pixel = ivec2(gl_GlobalInvocationID);
    if (pixel.x > imageSize(resolveImage).x ||
        pixel.y > imageSize(resolveImage).y)
        return;

    vec2 texCoord = (vec2(pixel) + 0.5) / vec2(imageSize(resolveImage));

    float depth = texelFetch(depthTexture, pixel, 0).r;

    vec4 volumetric;
    if (downsampled2x) {
        volumetric = Upsample2x(depth, pixel, texCoord);
    }
    else {
        volumetric = vec4(textureLod(lowResVolumetricTexture, texCoord, 0).rgb, 0.0);
    }

	vec3 viewPosition = ConvertDepthToViewSpace(depth, texCoord);
	vec3 worldPosition = vec3(ivMatrix * vec4(viewPosition, 1.0));

    vec4 resolve = imageLoad(resolveImage, pixel);
	

	float fogAmount = fogEnabled ? saturate(ComputeVolumetricFog(cameraLocation, worldPosition)) : 0.0;
    resolve = fogEnabled ? mix(vec4(fogColor, 1.0), resolve, fogAmount) + volumetric : resolve + volumetric;

    imageStore(resolveImage, pixel, resolve);

}