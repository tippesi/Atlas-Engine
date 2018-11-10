#include "Shadow.h"
#include "Light.h"

Shadow::Shadow(float distance, float bias, int32_t numCascades) :
	numCascades(numCascades), distance(distance), bias(bias) {

	cascades = new ShadowCascade[numCascades];

	for (int32_t i = 0; i < numCascades; i++) {
		cascades[i].nearDistance = (float)i * distance / (float)numCascades;
		cascades[i].farDistance = (float)(i + 1) * distance / (float)numCascades;
		cascades[i].map = new Framebuffer(1024, 1024);
		cascades[i].map->AddComponent(GL_DEPTH_ATTACHMENT, GL_FLOAT, GL_DEPTH_COMPONENT32F, GL_CLAMP_TO_EDGE, GL_LINEAR);
	}

	filtering = true;
	sampleCount = 16;
	sampleRange = 4;

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

	// A near enough up vector. This is because if the light location is 
	// (0.0f, 1.0f, 0.0f) the shadows wouldn't render correctly due to the 
	// shadows (or lights) view matrix. This is just a hack
	vec3 up = glm::vec3(0.0000000000000001f, 1.0f, 0.0000000000000001f);
	cascade->viewMatrix = lookAt(cascadeCenter, cascadeCenter + lightDirection, up);

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

	cascade->projectionMatrix = glm::ortho(minProj.x, maxProj.x, minProj.y, maxProj.y, -maxProj.z - 150.0f, -minProj.z);

}