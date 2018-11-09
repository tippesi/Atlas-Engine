#ifndef CAMERA_H
#define CAMERA_H

#include "System.h"
#include <vector>

/// <summary>
/// Used to represent a camera in the engine.
/// In third person mode the camera location represents the point the camera looks at while
/// in first person mode the camera is at the actual location.
/// </summary>
class Camera {
public:
	/// <summary>
	/// Constructs a <see cref="Camera"/>.
	/// </summary>
	/// <param name="fieldOfView">The field of view in degrees.</param>
	/// <param name="aspectRatio">The ratio of the image width to the image height.</param>
	/// <param name="nearPlane">The plane where the camera starts to render.</param>
	/// <param name="farPlane">The plane where the camera stops to render.</param>
	/// <param name="location">The location of the camera.</param>
	/// <param name="rotation">The rotation of the camera, where .X is the horizontal and .Y the vertical rotation.</param>
	Camera(float fieldOfView, float aspectRatio, float nearPlane, float farPlane, glm::vec3 location = vec3(0.0f), vec2 rotation = vec2(0.0f));

	/// <summary>
	/// Calculates the view matrix based on the location and rotation of the camera.
	/// </summary>
	void UpdateView();

	/// <summary>
	/// Calculates the perspective matrix based on the FoV, the aspect ratio and the near and far plane.
	/// </summary>
	void UpdateProjection();

	/// <summary>
	/// Calculates the view frustum corners in world space
	/// <param name="near">The near plane where the corners should be calculated
	/// <param name="far">The far plane where the corners should be calculated
	/// <returns> A vector where the corners are stored
	/// </summary>
	vector<vec3> GetFrustumCorners(float nearPlane, float farPlane);

	/// <summary>
	/// Destructs a <see cref="Camera"/>
	/// </summary>
	~Camera();

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

};

#endif
