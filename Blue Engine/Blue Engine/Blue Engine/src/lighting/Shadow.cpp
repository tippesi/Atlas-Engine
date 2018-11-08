#include "Shadow.h"

Shadow::Shadow(float distance, float bias, int32_t numCascades) :
	numCascades(numCascades), distance(distance), bias(bias) {

	cascades = new ShadowCascade[numCascades];

	filtering = true;
	numSamples = 16;
	sampleRange = 2.2f;

}

void Shadow::Update(Camera* camera) {

	for (uint32_t i = 0; i < numCascades; i++) {
		
	}

}

void Shadow::UpdateShadowCascade(ShadowCascade* cascade, Camera* camera) {

	vec3 cameraLocation = camera->thirdPerson ? camera->location - camera->direction * camera->thirdPersonDistance : camera->location;

	vec3 cascadeCenter = cameraLocation + camera->direction * (cascade->nearDistance + (cascade->farDistance - cascade->nearDistance) * 0.5f);

	vec3 lightDirection = glm::normalize(light->direction);

}