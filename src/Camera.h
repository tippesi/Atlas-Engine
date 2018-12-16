#ifndef CAMERA_H
#define CAMERA_H

#include "System.h"
#include <vector>

/**
 * Used to represent a camera in the engine.
 * In third person mode the camera location represents the point the camera looks at while
 * in first person mode the camera is at the actual location.
 */
class Camera {

public:
	/**
	 * Constructs a Camera object.
	 * @param fieldOfView The field of view in degrees.
	 * @param aspectRatio The ratio of the image width to the image height.
	 * @param nearPlane The plane where the camera starts to render.
	 * @param farPlane The plane where the camera stops to render.
	 * @param location The location of the camera.
	 * @param rotation The rotation of the camera, where .X is the horizontal and .Y the vertical rotation.
	 */
	Camera(float fieldOfView, float aspectRatio, float nearPlane, float farPlane, glm::vec3 location = vec3(0.0f), vec2 rotation = vec2(0.0f));

	/**
	 * Calculates the view matrix based on the location and rotation of the camera.
	 */
	void UpdateView();

	/**
	 * Calculates the perspective matrix based on the FoV, the aspect ratio and the near and far plane.
	 */
	void UpdateProjection();

	/**
	 * Calculates the view frustum corners in world space.
	 * @param nearPlane The near plane where the corners should be calculated.
	 * @param farPlane The far plane where the corners should be calculated.
	 * @return A vector where the corners are stored.
	 */
	vector<vec3> GetFrustumCorners(float nearPlane, float farPlane);

	/**
	 * Destructs a Camera object.
	 */
	~Camera();

	typedef struct Frustum {
		vec4 planes[6];
	}Frustum;

	vec3 location;
	vec2 rotation;

	float fieldOfView;
	float aspectRatio;
	float nearPlane;
	float farPlane;

	bool thirdPerson;
	float thirdPersonDistance;

	vec3 direction;
	vec3 up;
	vec3 right;

	mat4 viewMatrix;
	mat4 projectionMatrix;

	mat4 inverseViewMatrix;
	mat4 inverseProjectionMatrix;

	Frustum frustum;

private:
	void CalculateFrustum();

};

#endif
