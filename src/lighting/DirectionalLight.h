#ifndef DIRECTIONALLIGHT_H
#define DIRECTIONALLIGHT_H

#include "../System.h"
#include "ILight.h"

class DirectionalLight : public ILight {

public:
	DirectionalLight(int32_t mobility = MOVABLE_LIGHT);

	void AddShadow(float distance, float bias, int32_t resolution, int32_t cascadeCount, float splitCorrection, Camera* camera);

	void AddShadow(float distance, float bias, int32_t resolution, vec3 centerPoint, mat4 orthoProjection);

	void RemoveShadow();

	void AddVolumetric(Volumetric* volumetric);

	void RemoveVolumetric();

	void Update(Camera* camera);

	void ClearContent();

	void DeleteContent();

	vec3 direction;

private:
	void UpdateShadowCascade(ShadowComponent* cascade, Camera* camera);

	float FrustumSplitFormula(float correction, float near, float far, float splitIndex, float splitCount);

	vec3 shadowCenter;
	bool useShadowCenter;

};

#endif