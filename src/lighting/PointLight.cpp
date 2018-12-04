#include "PointLight.h"

PointLight::PointLight(int32_t mobility) {

    location = vec3(0.0f, 3.0f, 0.0f);

    color = vec3(1.0f);
    ambient = 0.1f;

    shadow = nullptr;
    volumetric = nullptr;

    type = POINT_LIGHT;
    this->mobility = mobility;

}

void PointLight::AddShadow(Shadow* shadow, Camera* camera) {

    this->shadow = shadow;
    shadow->light = this;



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