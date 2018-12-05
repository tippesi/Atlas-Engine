#ifndef POINTLIGHT_H
#define POINTLIGHT_H

#include "../System.h"
#include "ILight.h"

class PointLight : public ILight {

public:
    PointLight(int32_t mobility = STATIONARY_LIGHT);

    void AddShadow(float bias, int32_t resolution);

    void RemoveShadow();

    void AddVolumetric(Volumetric* volumetric);

    void RemoveVolumetric();

	void Update(Camera* camera);

    float GetRadius();

    vec3 location;

    float attenuation;

private:
    float radius;

};


#endif
