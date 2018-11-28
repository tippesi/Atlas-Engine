#include "Shadow.h"
#include "Light.h"

Shadow::Shadow(float distance, float bias, int32_t resolution, int32_t cascadeCount, float splitCorrection) : 
	distance(distance), bias(bias), resolution(resolution) {

	cascadeCount = glm::min(cascadeCount, MAX_SHADOW_CASCADE_COUNT);
	splitCorrection = glm::clamp(splitCorrection, 0.0f, 1.0f);
	componentCount = cascadeCount;
	this->splitCorrection = splitCorrection;

	sampleCount = 16;
	sampleRange = 2.2f;

	components = new ShadowComponent[cascadeCount];

	maps = new Texture(GL_UNSIGNED_INT, resolution, resolution, GL_DEPTH_COMPONENT24, 0.0f,
		GL_CLAMP_TO_EDGE, GL_LINEAR, false, false, cascadeCount);

}

Shadow::Shadow(float distance, float bias, int32_t resolution) :
	distance(distance), bias(bias), resolution(resolution) {

	componentCount = 1;
	sampleCount = 16;
	sampleRange = 2;
	splitCorrection = 0.0f;

	components = new ShadowComponent[componentCount];

	maps = new Texture(GL_UNSIGNED_INT, resolution, resolution, GL_DEPTH_COMPONENT24, 0.0f,
		GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);

}

void Shadow::Update(Camera* camera) {

	for (int32_t i = 0; i < componentCount; i++) {
		UpdateShadowComponent(&components[i], camera);
	}

}

Shadow::~Shadow() {

	delete[] components;
	delete maps;

}

void Shadow::UpdateShadowComponent(ShadowComponent* cascade, Camera* camera) {

	vec3 cameraLocation = camera->thirdPerson ? camera->location - camera->direction * camera->thirdPersonDistance : camera->location;

	vec3 cascadeCenter = cameraLocation + camera->direction * (cascade->nearDistance + (cascade->farDistance - cascade->nearDistance) * 0.5f);

	vec3 lightDirection = normalize(light->direction);

	// A near enough up vector. This is because if the light location is 
	// (0.0f, 1.0f, 0.0f) the shadows wouldn't render correctly due to the 
	// shadows (or lights) view matrix. This is just a hack
	vec3 up = glm::vec3(0.0000000000000001f, 1.0f, 0.0000000000000001f);
	cascade->viewMatrix = lookAt(cascadeCenter, cascadeCenter + lightDirection, up);

	vector<vec3> corners = camera->GetFrustumCorners(cascade->nearDistance, cascade->farDistance);

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