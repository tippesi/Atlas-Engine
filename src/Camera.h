#ifndef AE_CAMERA_H
#define AE_CAMERA_H

#include "System.h"
#include "volume/Frustum.h"

#include <vector>

namespace Atlas {

	/**
	 * Used to represent a camera in the engine.
	 * In third person mode the camera location represents the point the camera looks at while
	 * in first person mode the camera is at the actual location.
	 */
	class Camera {

	public:
		Camera() {}

		/**
         * Constructs a Camera object.
         * @param fieldOfView The field of view in degrees.
         * @param aspectRatio The ratio of the image width to the image height.
         * @param nearPlane The plane where the camera starts to render.
         * @param farPlane The plane where the camera stops to render.
         * @param location The location of the camera.
         * @param rotation The rotation of the camera, where .X is the horizontal and .Y the vertical rotation.
         */
		Camera(float fieldOfView, float aspectRatio, float nearPlane, float farPlane, glm::vec3 location = vec3(0.0f),
			   vec2 rotation = vec2(0.0f));

		/**
         * Calculates the view matrix based on the location and rotation of the camera.
         */
		void UpdateView();

		/**
         * Calculates the perspective matrix based on the FoV, the aspect ratio and the near and far plane.
         */
		void UpdateProjection();

		/**
		 * Calculates the actual camera location.
		 * @return The location as a 3-component vector.
		 */
		vec3 GetLocation();

		/**
         * Calculates the view frustum corners in world space.
         * @param nearPlane The near plane where the corners should be calculated.
         * @param farPlane The far plane where the corners should be calculated.
         * @return A vector where the corners are stored.
		 * @note The corners are in the following order
		 * with the far plane corners first and the near plane corners second:
		 * Far plane: Upper left, upper right, bottom left, bottom right
		 * Near plane: Upper left, upper right, bottom left, bottom right
         */
		std::vector<vec3> GetFrustumCorners(float nearPlane, float farPlane);

		vec3 location = vec3(0.0f);
		vec2 rotation = vec2(0.0f);

		float fieldOfView = 45.0f;
		float aspectRatio = 2.0f;
		float nearPlane = 1.0;
		float farPlane = 400.0f;

		bool thirdPerson = false;
		float thirdPersonDistance = 15.0f;

		vec3 direction = vec3(0.0f, 0.0f, -1.0f);
		vec3 up = vec3(0.0f, 1.0f, 0.0f);
		vec3 right = vec3(1.0f, 0.0f, 0.0f);

		mat4 viewMatrix;
		mat4 projectionMatrix;

		mat4 inverseViewMatrix;
		mat4 inverseProjectionMatrix;

		Volume::Frustum frustum;

	private:
		void CalculateFrustum();

	};

}

#endif
