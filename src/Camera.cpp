#include "Camera.h"

namespace Atlas {

	Camera::Camera(float fieldOfView, float aspectRatio, float nearPlane, float farPlane, vec3 location,
		vec2 rotation) : fieldOfView(fieldOfView), aspectRatio(aspectRatio), nearPlane(nearPlane),
		farPlane(farPlane), location(location), rotation(rotation) {

		UpdateView();
		UpdateProjection();

	}

	void Camera::UpdateView() {

		direction = normalize(vec3(cos(rotation.y) * sin(rotation.x),
			sin(rotation.y), cos(rotation.y) * cos(rotation.x)));

		right = normalize(vec3(sin(rotation.x - 3.14f / 2.0f), 
			0.0f, cos(rotation.x - 3.14f / 2.0f)));

		up = cross(right, direction);

		if (!thirdPerson)			
			viewMatrix = lookAt(location, location + direction, up);
		else
			viewMatrix = lookAt(location - direction * thirdPersonDistance, location, up);

		invViewMatrix = inverse(viewMatrix);

		UpdateFrustum();

	}

	void Camera::UpdateProjection() {

		projectionMatrix = glm::perspective(glm::radians(fieldOfView), aspectRatio, nearPlane, farPlane);
		invProjectionMatrix = inverse(projectionMatrix);

		unjitterdProjection = projectionMatrix;
		invUnjitteredProjection = invProjectionMatrix;

		UpdateFrustum();

	}

	void Camera::Update() {

		UpdateView();
		UpdateProjection();

	}

	void Camera::Jitter(vec2 jitter) {

		lastJitteredMatrix = jitteredMatrix;
		lastJitterVector = jitterVector;

		auto helper = glm::translate(vec3(jitter, 0.0f));
		projectionMatrix = helper * unjitterdProjection;

		invProjectionMatrix = inverse(projectionMatrix);

		UpdateFrustum();

		jitterVector = jitter;
		jitteredMatrix = projectionMatrix * viewMatrix;

	}

	vec2 Camera::GetJitter() {

		return jitterVector;

	}

	vec2 Camera::GetLastJitter() {

		return lastJitterVector;

	}

	mat4 Camera::GetLastJitteredMatrix() {

		return lastJitteredMatrix;

	}

	vec3 Camera::GetLocation() {

		return  thirdPerson ? location - direction * thirdPersonDistance : location;

	}

	std::vector<vec3> Camera::GetFrustumCorners(float nearPlane, float farPlane) {

		std::vector<vec3> corners;

		float radians = glm::radians(fieldOfView) / 2.0f;
		float tang = tanf(radians);

		float farHeight = farPlane * tang;
		float farWidth = aspectRatio * farHeight;
		float nearHeight = nearPlane * tang;
		float nearWidth = aspectRatio * nearHeight;

		vec3 cameraLocation = GetLocation();
		vec3 farPoint = cameraLocation + direction * farPlane;
		vec3 nearPoint = cameraLocation + direction * nearPlane;

		corners.push_back(farPoint + farHeight * up - farWidth * right);
		corners.push_back(farPoint + farHeight * up + farWidth * right);
		corners.push_back(farPoint - farHeight * up - farWidth * right);
		corners.push_back(farPoint - farHeight * up + farWidth * right);

		corners.push_back(nearPoint + nearHeight * up - nearWidth * right);
		corners.push_back(nearPoint + nearHeight * up + nearWidth * right);
		corners.push_back(nearPoint - nearHeight * up - nearWidth * right);
		corners.push_back(nearPoint - nearHeight * up + nearWidth * right);

		return corners;

	}

	void Camera::UpdateFrustum() {

		auto corners = GetFrustumCorners(nearPlane, farPlane);

		frustum.Resize(corners);

	}

}