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

		inverseViewMatrix = inverse(viewMatrix);

		CalculateFrustum();

	}

	void Camera::UpdateProjection() {

		projectionMatrix = glm::perspective(glm::radians(fieldOfView), aspectRatio, nearPlane, farPlane);
		inverseProjectionMatrix = inverse(projectionMatrix);

		unjitterdProjection = projectionMatrix;

		CalculateFrustum();

	}

	void Camera::Update() {

		UpdateView();
		UpdateProjection();

	}

	void Camera::Jitter(vec2 jitter) {

		auto helper = glm::translate(mat4(1.0f), vec3(jitter, 0.0f));
		projectionMatrix = helper * projectionMatrix;

		jitterVector = jitter;

		inverseProjectionMatrix = inverse(projectionMatrix);

		CalculateFrustum();

	}

	void Camera::Unjitter() {

		auto helper = glm::translate(mat4(1.0f), vec3(-jitterVector, 0.0f));
		projectionMatrix = helper * projectionMatrix;

		jitterVector = vec2(0.0f);

		inverseProjectionMatrix = inverse(projectionMatrix);

		CalculateFrustum();

	}

	vec2 Camera::GetJitter() {

		return jitterVector;

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

	void Camera::CalculateFrustum() {

		auto corners = GetFrustumCorners(nearPlane, farPlane);

		frustum.Resize(corners);

	}

}