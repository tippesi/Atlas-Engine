#include "DirectionalLight.h"

DirectionalLight::DirectionalLight(int32_t mobility) {

	direction = vec3(0.0f, -1.0f, 0.0f);

	color = vec3(1.0f);
	ambient = 0.1f;

	shadow = nullptr;
	volumetric = nullptr;

	type = DIRECTIONAL_LIGHT;
	this->mobility = mobility;

}

void DirectionalLight::AddShadow(Shadow* shadow, Camera* camera) {

	this->shadow = shadow;
	shadow->light = this;

    // We want cascaded shadow mapping for directional lights
    for (int32_t i = 0; i < shadow->componentCount; i++) {
        shadow->components[i].nearDistance = FrustumSplitFormula(shadow->splitCorrection, camera->nearPlane, shadow->distance,
                                                                 (float)i, (float)shadow->componentCount);
        shadow->components[i].farDistance = FrustumSplitFormula(shadow->splitCorrection, camera->nearPlane, shadow->distance,
                                                                (float)i + 1, (float)shadow->componentCount);
    }

}

void DirectionalLight::RemoveShadow() {

	shadow = nullptr;

}

void DirectionalLight::AddVolumetric(Volumetric *volumetric) {

	this->volumetric = volumetric;

}

void DirectionalLight::RemoveVolumetric() {

	volumetric = nullptr;

}

void DirectionalLight::ClearContent() {

	shadow = nullptr;
	volumetric = nullptr;

}

void DirectionalLight::DeleteContent() {

	delete shadow;
	delete volumetric;

	ClearContent();

}

float DirectionalLight::FrustumSplitFormula(float correction, float near, float far, float splitIndex, float splitCount) {

	return correction * near * powf(far / near, splitIndex / splitCount) +
		(1.0f - correction) * (near + (splitIndex / splitCount) * (far - near));

}