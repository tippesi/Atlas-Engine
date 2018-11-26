#include "Volumetric.h"

Volumetric::Volumetric(int32_t width, int32_t height, int32_t sampleCount, float scattering, float scatteringFactor) :
	sampleCount(sampleCount), scattering(scattering), scatteringFactor(scatteringFactor) {

    map = new Texture(GL_FLOAT, width, height, GL_R16F, 0.0f, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);
    blurMap = new Texture(GL_FLOAT, width, height, GL_R16F, 0.0f, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);

}

Volumetric::~Volumetric() {

	delete map;
	delete blurMap;

}