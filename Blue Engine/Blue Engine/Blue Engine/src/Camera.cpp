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

	direction = normalize(vec3(cos(rotation.y) * sin(rotation.x), sin(rotation.y), cos(rotation.y) * cos(rotation.x)));

	right = vec3(sin(rotation.x - 3.14f / 2.0f), 0.0f, cos(rotation.x - 3.14f / 2.0f));

	if (!thirdPerson) {

		up = cross(right, direction);
		viewMatrix = lookAt(location, location + direction, up);

	}
	else {

		up = cross(right, -direction);
		viewMatrix = lookAt(location - direction * thirdPersonDistance, location, -up);

	}

	inverseViewMatrix = mat4(inverse(viewMatrix));

}

void Camera::UpdateProjection() {

	projectionMatrix = glm::perspective(glm::radians(fieldOfView), aspectRatio, nearPlane, farPlane);
	inverseProjectionMatrix = inverse(projectionMatrix);

}

vector<vec3> Camera::GetFrustumCorners(float nearPlane, float farPlane) {

	vector<vec3> corners;

	float tang = tanf(glm::radians(fieldOfView));

	float farHeight = farPlane * tang;
	float farWidth = aspectRatio * farHeight;
	float nearHeight = nearPlane * tang;
	float nearWidth = aspectRatio * nearHeight;

	vec3 cameraLocation = thirdPerson ? location - direction * thirdPersonDistance : location;
	vec3 far = cameraLocation + direction * farPlane;
	vec3 near = cameraLocation + direction * nearPlane;

	corners.push_back(far + farHeight * up - farWidth * right);
	corners.push_back(far + farHeight * up + farWidth * right);
	corners.push_back(far - farHeight * up - farWidth * right);
	corners.push_back(far - farHeight * up + farWidth * right);

	corners.push_back(near + nearHeight * up - nearWidth * right);
	corners.push_back(near + nearHeight * up + nearWidth * right);
	corners.push_back(near - nearHeight * up - nearWidth * right);
	corners.push_back(near - nearHeight * up + nearWidth * right);

	return corners;

}

Camera::~Camera() {



}
