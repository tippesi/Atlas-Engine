#include "Light.h"

Light::Light(int32_t type) : type(type) {

	direction = vec3(0.0f, -1.0f, 0.0f);
	location = vec3(0.0f, 3.0f, 0.0f);

	diffuseColor = vec3(1.0f);
	ambient = 0.1f;

	shadow = nullptr;
	volumetric = nullptr;

}

void Light::AddShadow(Shadow* shadow, Camera* camera) {

	this->shadow = shadow;
	shadow->light = this;

	// Generate different shadow components depending on the light type
	if (type == DIRECTIONAL_LIGHT) {
		// We want cascaded shadow mapping for directional lights
		for (int32_t i = 0; i < shadow->componentCount; i++) {
			shadow->components[i].nearDistance = FrustumSplitFormula(shadow->splitCorrection, camera->nearPlane, shadow->distance, 
				i, shadow->componentCount);
			shadow->components[i].farDistance = FrustumSplitFormula(shadow->splitCorrection, camera->nearPlane, shadow->distance,
				i + 1, shadow->componentCount);
			shadow->components[i].map = new Framebuffer(1024, 1024);
			shadow->components[i].map->AddComponent(GL_DEPTH_ATTACHMENT, GL_UNSIGNED_INT, GL_DEPTH_COMPONENT24, GL_CLAMP_TO_EDGE, GL_LINEAR);
		}
	}
	else if (type == POINT_LIGHT) {

	}
	else if (type == SPOT_LIGHT) {

	}

}

float Light::FrustumSplitFormula(float correction, float near, float far, float splitIndex, float splitCount) {

	return correction * near * powf(far / near, splitIndex / splitCount) +
		(1.0f - correction) * (near + (splitIndex / splitCount) * (far - near));

}