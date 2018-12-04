#ifndef ILIGHT_H
#define ILIGHT_H

#include "../System.h"
#include "Shadow.h"
#include "Volumetric.h"

#define STATIC_LIGHT 0
#define DYNAMIC_LIGHT 1

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

class ILight {

    virtual void AddShadow(Shadow* shadow, Camera* camera) = 0;

    virtual void RemoveShadow() = 0;

    virtual void AddVolumetric(Volumetric* volumetric) = 0;

    virtual void RemoveVolumetric() = 0;

public:
    int32_t type;
    int32_t mobility;

    vec3 color;
    float ambient;

    Shadow* shadow;
    Volumetric* volumetric;

};


#endif
