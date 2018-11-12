#ifndef VOLUMETRIC_H
#define VOLUMETRIC_H

#include "../System.h"
#include "../Texture.h"

class Volumetric {

public:
	Volumetric(int32_t width, int32_t height, int32_t sampleCount);

	int32_t sampleCount;

    Texture* map;

};

#endif