#include "Camera.h"


Camera::Camera(float fieldOfView, float aspectRatio, float nearPlane, float farPlane, vec3 location, vec2 rotation) {

	this->fieldOfView = fieldOfView;
	this->aspectRatio = aspectRatio;
	this->nearPlane = nearPlane;
	this->farPlane = farPlane;

	this->location = location;
	this->rotation = rotation;

	thirdPerson = false;
	thirdPersonDistance = 10.0f;

	UpdateView();
	UpdateProjection();

}

void Camera::UpdateView() {

	direction = glm::normalize(glm::vec3(cos(rotation.y) * sin(rotation.x), sin(rotation.y), cos(rotation.y) * cos(rotation.x)));

	right = glm::vec3(sin(rotation.x - 3.14f / 2.0f), 0.0f, cos(rotation.x - 3.14f / 2.0f));

	if (!thirdPerson) {

		up = glm::cross(right, direction);
		viewMatrix = glm::lookAt(location, location + direction, up);

	}
	else {

		up = glm::cross(right, -direction);
		viewMatrix = glm::lookAt(location - direction * thirdPersonDistance, location, -up);

	}

	inverseViewMatrix = glm::mat4(glm::inverse(viewMatrix));

}

void Camera::UpdateProjection() {

	projectionMatrix = glm::perspective(glm::radians(fieldOfView), aspectRatio, nearPlane, farPlane);
	inverseProjectionMatrix = glm::inverse(projectionMatrix);

}

Camera::~Camera() {



}
