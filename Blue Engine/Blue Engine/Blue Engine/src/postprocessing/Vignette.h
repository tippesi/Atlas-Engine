#ifndef VIGNETTE_H
#define VIGNETTE_H

#include "../System.h"

class Vignette {

public:
	Vignette();

	float offset;
	float power;
	float strength;

	vec3 color;

	bool use;

};

#endif