#include <../common/octahedron.hsh>
#include <../common/utility.hsh>
#include <../brdf/preintegrate.hsh>
#include <../brdf/filtering.hsh>
#include <../brdf/importanceSample.hsh>

#ifdef IRRADIANCE
layout (location = 0) out vec4 irradianceFS;
#else
layout (location = 0) out vec2 momentsFS;
#endif

uniform ivec3 probeOffset;
uniform int probeRes;

vec2 GetCoords(ivec2 fragCoord) {

	int probeResSide = probeRes + 2;

	ivec2 octFragCoord = ivec2((fragCoord.x - 1) % probeResSide, (fragCoord.y - 1) % probeResSide);
	return (vec2(octFragCoord)) / float(probeRes - 1.0);

}

void main() {

	vec2 coord = GetCoords(ivec2(gl_FragCoord.xy));

	// Generate normal of the current texel
	vec3 N = OctahedronToUnitVector(coord);

#ifdef IRRADIANCE
	irradianceFS = Irradiance(N);
#else
	// We now randomly sample in the area of the texel
	// and generate directions.
	const uint sampleCount = 5000u;
	const float depthSharpness = 50.0;

	// http://www.punkuser.net/vsm/vsm_paper.pdf
	float sumR = 0.0;
	float sumR2 = 0.0;

	vec3 dir = OctahedronToUnitVector(coord);
	float sumWeights = 0.0;

	for (uint i = 0; i < sampleCount; i++) {
		
		vec2 Xi = Hammersley(i, sampleCount);
		
		vec3 L;
        float NdotL;
        float pdf;

        ImportanceSampleCosDir(N, Xi, L, NdotL, pdf);

		float weight = pow(max(0.0, NdotL), depthSharpness) / pdf;
		float depth = texture(probe, L).r * weight;

		sumR += depth;
		sumR2 += sqr(depth);
		sumWeights += weight;

	}

	sumR /= sumWeights;
	sumR2 /= sumWeights;

	momentsFS = vec2(sumR, sumR2);
#endif

}