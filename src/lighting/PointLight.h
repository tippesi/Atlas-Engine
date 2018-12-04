#ifndef POINTLIGHT_H
#define POINTLIGHT_H

#include "../System.h"
#include "ILight.h"


class PointLight : public ILight {

public:
    PointLight(int32_t mobility = STATIONARY_LIGHT);

    void AddShadow(Shadow* shadow, Camera* camera);

    void RemoveShadow();

    void AddVolumetric(Volumetric* volumetric);

    void RemoveVolumetric();

    float GetRadius();

    vec3 location;

    float attenuation;

private:
    float radius;

};


#endif
