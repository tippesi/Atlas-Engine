#ifndef AE_VOLUMETRIC_H
#define AE_VOLUMETRIC_H

#include "../System.h"
#include "../texture/Texture2D.h"

class Volumetric {

public:
	Volumetric(int32_t width, int32_t height, int32_t sampleCount, float scattering, float scatteringFactor = 1.0f);

	~Volumetric();

	int32_t sampleCount;
	float scattering;
	float scatteringFactor;

    Texture2D* map;
    Texture2D* blurMap;

};

#endif