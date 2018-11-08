#include "Shadow.h"
#include "Light.h"

Shadow::Shadow(float distance, float bias, int32_t numCascades) :
	numCascades(numCascades), distance(distance), bias(bias) {

	cascades = new ShadowCascade[numCascades];

	filtering = true;
	numSamples = 16;
	sampleRange = 2.2f;

}

void Shadow::Update(Camera* camera) {

	for (int32_t i = 0; i < numCascades; i++) {
		UpdateShadowCascade(&cascades[i], camera);
	}

}

void Shadow::UpdateShadowCascade(ShadowCascade* cascade, Camera* camera) {

	vec3 cameraLocation = camera->thirdPerson ? camera->location - camera->direction * camera->thirdPersonDistance : camera->location;

	vec3 cascadeCenter = cameraLocation + camera->direction * (cascade->nearDistance + (cascade->farDistance - cascade->nearDistance) * 0.5f);

	vec3 lightDirection = normalize(light->direction);

	cascade->viewMatrix = lookAt(cascadeCenter, cascadeCenter + lightDirection, vec3(0.0f, 1.0f, 0.0f));

	vector<vec3> corners = camera->GetFrustumCorners(cascade->nearDistance, cascade->farDistance);

	vec3 maxProj = corners.at(0);
	vec3 minProj = corners.at(0);

	for (auto corner : corners) {
		corner = vec3(cascade->viewMatrix * vec4(corner, 1.0f));

		maxProj.x = glm::max(maxProj.x, corner.x);
		maxProj.y = glm::max(maxProj.y, corner.y);
		maxProj.z = glm::max(maxProj.z, corner.z);

		minProj.x = glm::min(minProj.x, corner.x);
		minProj.y = glm::min(minProj.y, corner.y);
		minProj.z = glm::min(minProj.z, corner.z);
	}

	cascade->projectionMatrix = glm::ortho(minProj.x, maxProj.x, minProj.y, maxProj.y, -maxProj.z, -minProj.z);

}