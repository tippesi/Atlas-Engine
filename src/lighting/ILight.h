#ifndef ILIGHT_H
#define ILIGHT_H

#include "../System.h"
#include "Shadow.h"
#include "Volumetric.h"

#define STATIONARY_LIGHT 0
#define MOVABLE_LIGHT 1

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

class ILight {

public:
    virtual void RemoveShadow() = 0;

    virtual void RemoveVolumetric() = 0;

	virtual void Update(Camera* camera) = 0;

	inline Shadow * GetShadow() {
		return shadow;
	}

	inline Volumetric* GetVolumetric() {
		return volumetric;
	}

    int32_t type;
    int32_t mobility;

    vec3 color;
    float ambient;

protected:
    Shadow* shadow;
    Volumetric* volumetric;

};


#endif
