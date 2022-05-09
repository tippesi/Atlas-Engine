#include <deferred.hsh>

#include <../ddgi/ddgi.hsh>
#include <../brdf/brdfEval.hsh>

layout (local_size_x = 8, local_size_y = 8) in;

layout(binding = 0, rgba16f) uniform image2D image;

uniform mat4 ivMatrix;

uniform float indirectStrength = 1.0;
uniform bool aoEnabled = true;

void main() {

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
	vec3 prefilteredDiffuse = texture(diffuseProbe, worldNormal).rgb;
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
	vec3 indirectSpecular = prefilteredSpecular * EvaluateIndirectSpecularBRDF(surface)
		* prefilteredDiffuseLocal.a;

	vec3 indirect = (indirectDiffuse + indirectSpecular) * surface.material.ao * indirectStrength;
	
	// This normally only accounts for diffuse occlusion, we need seperate terms
	// for diffuse and specular.
	float occlusionFactor = aoEnabled ? texture(aoTexture, texCoord).r : 1.0;
	indirect *= occlusionFactor;

    vec3 direct = imageLoad(image, pixel).rgb;
    imageStore(image, pixel, vec4(direct + indirect, 0.0));

}