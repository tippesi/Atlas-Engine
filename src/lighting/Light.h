#ifndef LIGHT_H
#define LIGHT_H

#include "../System.h"
#include "Shadow.h"
#include "Volumetric.h"

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

class Light {

public:
	Light(int32_t type);

	void AddShadow(Shadow* shadow, Camera* camera);

	void RemoveShadow();

	void AddVolumetric(Volumetric* volumetric);

	void RemoveVolumetric();

	void ClearContent();

	void DeleteContent();

	int32_t type;

	vec3 location;
	vec3 direction;
	
	vec3 diffuseColor;
	float ambient;

	Shadow* shadow;
	Volumetric* volumetric;

private:
	float FrustumSplitFormula(float correction, float near, float far, float splitIndex, float splitCount);

};

#endif