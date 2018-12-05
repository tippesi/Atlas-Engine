#include "PointLight.h"

PointLight::PointLight(int32_t mobility) {

    location = vec3(0.0f, 3.0f, 0.0f);

    color = vec3(1.0f);
    ambient = 0.1f;
	radius = 5.0f;

    shadow = nullptr;
    volumetric = nullptr;

    type = POINT_LIGHT;
    this->mobility = mobility;

}

void PointLight::AddShadow(float bias, int32_t resolution) {

    this->shadow = new Shadow(0.0f, bias, resolution, true);

	mat4 projectionMatrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, radius);
	vec3 faces[] = { vec3(1.0f, 0.0f, 0.0f), vec3(-1.0f, 0.0f, 0.0f),
					vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f),
					vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f) };

	vec3 ups[] = { vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f),
					vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f),
					vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f) };

	for (uint8_t i = 0; i < 6; i++) {
		shadow->components[i].projectionMatrix = projectionMatrix;
		shadow->components[i].viewMatrix = glm::lookAt(location, location + faces[i], ups[i]);
	}

}

void PointLight::RemoveShadow() {

    shadow = nullptr;

}

void PointLight::AddVolumetric(Volumetric *volumetric) {

    this->volumetric = volumetric;

}

void PointLight::RemoveVolumetric() {

    volumetric = nullptr;

}

void PointLight::Update(Camera* camera) {

	if (mobility == MOVABLE_LIGHT && shadow != nullptr) {
		shadow->Update();
	}

}

float PointLight::GetRadius() {

	return radius;

}