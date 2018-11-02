#ifndef LIGHT_H
#define LIGHT_H

#include "system.h"

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

class Light {

public:
	Light(int32_t type);

	int32_t type;

};

#endif