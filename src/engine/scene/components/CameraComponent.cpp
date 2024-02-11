#include "CameraComponent.h"

namespace Atlas {

	namespace Scene {

		namespace Components {

            CameraComponent::CameraComponent(float fieldOfView, float aspectRatio, float nearPlane, float farPlane, vec3 location,
                vec2 rotation) : fieldOfView(fieldOfView), aspectRatio(aspectRatio), nearPlane(nearPlane),
                farPlane(farPlane), location(location), rotation(rotation) {

                UpdateView(mat4 {1.0f});
                UpdateProjection();

            }

            void CameraComponent::UpdateView(mat4 transform) {

                lastViewMatrix = viewMatrix;

                direction = normalize(vec3(cos(rotation.y) * sin(rotation.x),
                    sin(rotation.y), cos(rotation.y) * cos(rotation.x)));

                right = normalize(vec3(sin(rotation.x - 3.14f / 2.0f),
                    0.0f, cos(rotation.x - 3.14f / 2.0f)));

                up = cross(right, direction);

                if (!thirdPerson)
                    viewMatrix = lookAt(location, location + direction, up) * glm::inverse(transform);
                else
                    viewMatrix = lookAt(location - direction * thirdPersonDistance, location, up) * glm::inverse(transform);

                invViewMatrix = inverse(viewMatrix);

                UpdateFrustum();

            }

            void CameraComponent::UpdateProjection() {

                const mat4 clip = mat4(1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, -1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.5f, 0.0f,
                    0.0f, 0.0f, 0.5f, 1.0f);

                projectionMatrix = clip * glm::perspective(glm::radians(fieldOfView), aspectRatio, nearPlane, farPlane);
                invProjectionMatrix = inverse(projectionMatrix);

                unjitterdProjection = projectionMatrix;
                invUnjitteredProjection = invProjectionMatrix;

                UpdateFrustum();

            }

            void CameraComponent::Update(mat4 transform) {

                UpdateView(transform);
                UpdateProjection();

            }

            void CameraComponent::Jitter(vec2 jitter) {

                lastJitteredMatrix = jitteredMatrix;
                lastJitterVector = jitterVector;

                auto helper = glm::translate(vec3(jitter, 0.0f));
                projectionMatrix = helper * unjitterdProjection;

                invProjectionMatrix = inverse(projectionMatrix);

                UpdateFrustum();

                jitterVector = jitter;
                jitteredMatrix = projectionMatrix * viewMatrix;

            }

            vec2 CameraComponent::GetJitter() const {

                return jitterVector;

            }

            vec2 CameraComponent::GetLastJitter() const {

                return lastJitterVector;

            }

            mat4 CameraComponent::GetLastJitteredMatrix() const {

                return lastJitteredMatrix;

            }

            vec3 CameraComponent::GetLocation() const {

                return vec3(invViewMatrix[3]);

            }

            vec3 CameraComponent::GetLastLocation() const {

                return vec3(glm::inverse(lastViewMatrix)[3]);

            }

            std::vector<vec3> CameraComponent::GetFrustumCorners(float nearPlane, float farPlane) const {

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

            void CameraComponent::UpdateFrustum() {

                auto corners = GetFrustumCorners(nearPlane, farPlane);

                frustum.Resize(corners);

            }

		}

	}

}