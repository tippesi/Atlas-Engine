#include "DirectionalLight.h"

namespace Atlas {

	namespace Light {



	}

}

DirectionalLight::DirectionalLight(int32_t mobility) : ILight(AE_DIRECTIONAL_LIGHT, mobility) {

	direction = vec3(0.0f, -1.0f, 0.0f);

	color = vec3(1.0f);
	ambient = 0.1f;

	shadow = nullptr;
	volumetric = nullptr;

}

DirectionalLight::~DirectionalLight() {

	delete shadow;
	delete volumetric;

}

void DirectionalLight::AddShadow(float distance, float bias, int32_t resolution, int32_t cascadeCount, float splitCorrection, Camera* camera) {

	this->shadow = new Shadow(distance, bias, resolution, cascadeCount, splitCorrection);

	useShadowCenter = false;

    // We want cascaded shadow mapping for directional lights
    for (int32_t i = 0; i < shadow->componentCount; i++) {
        shadow->components[i].nearDistance = FrustumSplitFormula(shadow->splitCorrection, camera->nearPlane, shadow->distance,
                                                                 (float)i, (float)shadow->componentCount);
        shadow->components[i].farDistance = FrustumSplitFormula(shadow->splitCorrection, camera->nearPlane, shadow->distance,
                                                                (float)i + 1, (float)shadow->componentCount);
    }

}

void DirectionalLight::AddShadow(float distance, float bias, int32_t resolution, vec3 centerPoint, mat4 orthoProjection) {

	shadow = new Shadow(distance, bias, resolution);

	useShadowCenter = true;

	shadow->components[0].nearDistance = 0.0f;
	shadow->components[0].farDistance = distance;
	shadow->components[0].projectionMatrix = orthoProjection;
	shadow->components[0].viewMatrix = glm::lookAt(centerPoint, centerPoint + direction, vec3(0.0f, 1.0f, 0.0f));

}

void DirectionalLight::RemoveShadow() {

	delete shadow;
	shadow = nullptr;

}

void DirectionalLight::AddVolumetric(int32_t width, int32_t height, int32_t sampleCount, float scattering, float scatteringFactor) {

	volumetric = new Volumetric(width, height, sampleCount, scattering, scatteringFactor);

}

void DirectionalLight::RemoveVolumetric() {

	delete volumetric;
	volumetric = nullptr;

}

void DirectionalLight::Update(Camera* camera) {

	if (shadow != nullptr) {

		if (!useShadowCenter) {

			for (int32_t i = 0; i < shadow->componentCount; i++) {
				UpdateShadowCascade(&shadow->components[i], camera);
			}

		}
	
		if (mobility == AE_MOVABLE_LIGHT) {
			shadow->Update();
		}
	
	}

}

void DirectionalLight::UpdateShadowCascade(ShadowComponent* cascade, Camera* camera) {

	vec3 cameraLocation = camera->thirdPerson ? camera->location - camera->direction * camera->thirdPersonDistance : camera->location;

	vec3 cascadeCenter = cameraLocation + camera->direction * (cascade->nearDistance + (cascade->farDistance - cascade->nearDistance) * 0.5f);

	vec3 lightDirection = normalize(direction);

	// A near enough up vector. This is because if the light location is 
	// (0.0f, 1.0f, 0.0f) the shadows wouldn't render correctly due to the 
	// shadows (or lights) view matrix. This is just a hack
	vec3 up = glm::vec3(0.0000000000000001f, 1.0f, 0.0000000000000001f);
	cascade->viewMatrix = lookAt(cascadeCenter, cascadeCenter + lightDirection, up);

	std::vector<vec3> corners = camera->GetFrustumCorners(cascade->nearDistance, cascade->farDistance);

	vec3 maxProj = vec3(cascade->viewMatrix * vec4(corners.at(0), 1.0f));
	vec3 minProj = maxProj;

	for (auto corner : corners) {
		corner = vec3(cascade->viewMatrix * vec4(corner, 1.0f));

		maxProj.x = glm::max(maxProj.x, corner.x);
		maxProj.y = glm::max(maxProj.y, corner.y);
		maxProj.z = glm::max(maxProj.z, corner.z);

		minProj.x = glm::min(minProj.x, corner.x);
		minProj.y = glm::min(minProj.y, corner.y);
		minProj.z = glm::min(minProj.z, corner.z);
	}

	cascade->projectionMatrix = glm::ortho(minProj.x, maxProj.x, minProj.y, maxProj.y, -maxProj.z - 150.0f, -minProj.z);

}

float DirectionalLight::FrustumSplitFormula(float correction, float near, float far, float splitIndex, float splitCount) {

	return correction * near * powf(far / near, splitIndex / splitCount) +
		(1.0f - correction) * (near + (splitIndex / splitCount) * (far - near));

}