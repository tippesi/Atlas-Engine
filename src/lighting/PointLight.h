#ifndef POINTLIGHT_H
#define POINTLIGHT_H

#include "../System.h"
#include "ILight.h"

class PointLight : public ILight {

public:
    PointLight(int32_t mobility = AE_STATIONARY_LIGHT);

	~PointLight();

    void AddShadow(float bias, int32_t resolution);

    void RemoveShadow();

    void AddVolumetric(int32_t width, int32_t height, int32_t sampleCount, float scattering, float scatteringFactor = 1.0f);

    void RemoveVolumetric();

	void Update(Camera* camera);

    float GetRadius();

    vec3 location;

    float attenuation;

private:
    float radius;

};


#endif
