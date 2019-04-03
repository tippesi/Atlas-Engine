#include "Camera.h"

namespace Atlas {

	Camera::Camera(float fieldOfView, float aspectRatio, float nearPlane, float farPlane, vec3 location,
				   vec2 rotation) {

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

	Camera::~Camera() {


	}

	void Camera::UpdateView() {

		direction = normalize(
				vec3(cos(rotation.y) * sin(rotation.x), sin(rotation.y), cos(rotation.y) * cos(rotation.x)));

		right = vec3(sin(rotation.x - 3.14f / 2.0f), 0.0f, cos(rotation.x - 3.14f / 2.0f));

		if (!thirdPerson) {

			up = cross(right, direction);
			viewMatrix = lookAt(location, location + direction, up);

		} else {

			up = cross(right, -direction);
			viewMatrix = lookAt(location - direction * thirdPersonDistance, location, -up);

		}

		inverseViewMatrix = inverse(viewMatrix);

		CalculateFrustum();

	}

	void Camera::UpdateProjection() {

		projectionMatrix = glm::perspective(glm::radians(fieldOfView), aspectRatio, nearPlane, farPlane);
		inverseProjectionMatrix = inverse(projectionMatrix);

		CalculateFrustum();

	}

	std::vector<vec3> Camera::GetFrustumCorners(float nearPlane, float farPlane) {

		std::vector<vec3> corners;

		float radians = glm::radians(fieldOfView) / 2.0f;
		float tang = tanf(radians);

		float farHeight = farPlane * tang;
		float farWidth = aspectRatio * farHeight;
		float nearHeight = nearPlane * tang;
		float nearWidth = aspectRatio * nearHeight;

		vec3 cameraLocation = thirdPerson ? location - direction * thirdPersonDistance : location;
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

		vec4 rowVectors[4];

		mat4 matrix = projectionMatrix * viewMatrix;

		for (int32_t i = 0; i < 4; i++) {
			rowVectors[i] = vec4(matrix[0][i], matrix[1][i],
								 matrix[2][i], matrix[3][i]);
		}

		frustum.planes[0] = rowVectors[3] + rowVectors[2];
		frustum.planes[1] = rowVectors[3] + rowVectors[2];
		frustum.planes[2] = rowVectors[3] + rowVectors[0];
		frustum.planes[3] = rowVectors[3] + rowVectors[0];
		frustum.planes[4] = rowVectors[3] + rowVectors[1];
		frustum.planes[5] = rowVectors[3] + rowVectors[1];

		frustum.planes[0][3] -= frustum.planes[0][2];
		frustum.planes[1][3] += frustum.planes[1][2];
		frustum.planes[2][3] += frustum.planes[2][0];
		frustum.planes[3][3] -= frustum.planes[3][0];
		frustum.planes[4][3] += frustum.planes[4][1];
		frustum.planes[5][3] -= frustum.planes[5][1];

		for (int32_t i = 0; i < 6; i++) {
			frustum.planes[i] = normalize(inverseViewMatrix * frustum.planes[i]);
		}

	}

}