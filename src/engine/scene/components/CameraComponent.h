#pragma once

#include "../Entity.h"
#include "../../System.h"
#include "../../volume/Frustum.h"

namespace Atlas {

	namespace Scene {

        class Scene;

		namespace Components {

            class CameraComponent {

            public:
                CameraComponent() = default;
                CameraComponent(const CameraComponent& that) = default;

                CameraComponent(float fieldOfView, float aspectRatio, float nearPlane, float farPlane, glm::vec3 location = vec3(0.0f),
                    vec2 rotation = vec2(0.0f));

                void Jitter(vec2 jitter);

                vec2 GetJitter() const;

                vec2 GetLastJitter() const;

                mat4 GetLastJitteredMatrix() const;

                mat4 GetLastViewMatrix() const;

                vec3 GetLocation() const;

                vec3 GetLastLocation() const;

                std::array<vec3, 8> GetFrustumCorners(float nearPlane, float farPlane) const;

                void UpdateFrustum();

                vec3 location = vec3{ 0.0f };
                vec2 rotation = vec2{ 0.0f };

                float exposure = 1.0f;

                float fieldOfView = 45.0f;
                float aspectRatio = 2.0f;
                float nearPlane = 1.0;
                float farPlane = 400.0f;

                bool thirdPerson = false;
                float thirdPersonDistance = 15.0f;

                vec3 direction = vec3{ 0.0f, 0.0f, -1.0f };
                vec3 up = vec3{ 0.0f, 1.0f, 0.0f };
                vec3 right = vec3{ 1.0f, 0.0f, 0.0f };

                vec3 globalLocation = vec3{ 0.0f };
                vec3 globalDirection = vec3{ 0.0f, 0.0f, -1.0f };
                vec3 globalUp = vec3{ 0.0f, 1.0f, 0.0f };
                vec3 globalRight = vec3{ 1.0f, 0.0f, 0.0f };

                mat4 viewMatrix = mat4{ 1.0f };
                mat4 projectionMatrix = mat4{ 1.0f };

                mat4 invViewMatrix = mat4{ 1.0f };
                mat4 invProjectionMatrix = mat4{ 1.0f };

                mat4 unjitterdProjection = mat4{ 1.0f };
                mat4 invUnjitteredProjection = mat4{ 1.0f };

                mat4 parentTransform = mat4{ 1.0f };

                Volume::Frustum frustum;

                bool isMain = true;

                bool useEntityTranslation = true;
                bool useEntityRotation = true;

            private:
                void UpdateView(mat4 transform);

                void UpdateProjection();

                void Update(mat4 transform);

                vec2 jitterVector = vec2{ 0.0f };
                vec2 lastJitterVector = vec2{ 0.0f };

                mat4 jitteredMatrix = mat4{ 1.0f };
                mat4 lastJitteredMatrix = mat4{ 1.0f };

                mat4 lastViewMatrix = mat4{ 1.0f };

                friend Scene;

            };

		}

	}

}