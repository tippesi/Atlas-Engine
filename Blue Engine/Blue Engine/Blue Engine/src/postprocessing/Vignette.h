#ifndef VIGNETTE_H
#define VIGNETTE_H

#include "../System.h"

class Vignette {

public:
	Vignette(float offset, float power, float strength, vec3 color = vec3(0.0f)) :
		offset(offset), power(power), strength(strength), color(color) { };

	float offset;
	float power;
	float strength;

	vec3 color;

};

#endif