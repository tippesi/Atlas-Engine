#ifndef DIRECTIONALLIGHT_H
#define DIRECTIONALLIGHT_H

#include "../System.h"
#include "ILight.h"

class DirectionalLight : public ILight {

public:
	DirectionalLight(int32_t mobility = MOVABLE_LIGHT);

	void AddShadow(Shadow* shadow, Camera* camera);

	void RemoveShadow();

	void AddVolumetric(Volumetric* volumetric);

	void RemoveVolumetric();

	void ClearContent();

	void DeleteContent();

	vec3 direction;

private:
	float FrustumSplitFormula(float correction, float near, float far, float splitIndex, float splitCount);

};

#endif