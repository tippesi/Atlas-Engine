#pragma once

#include "../../System.h"
#include "../../physics/PhysicsWorld.h"
#include "../../physics/Player.h"

#include "TransformComponent.h"

namespace Atlas::Scene {

	class Scene;

	namespace Components {

		class PlayerComponent : public Physics::Player {

		public:
			PlayerComponent() = default;
			PlayerComponent(const PlayerComponent& that) = default;
            explicit PlayerComponent(float mass, float maxStrength);
			explicit PlayerComponent(const Physics::PlayerCreationSettings& playerCreationSettings);

			void Jump() { jumpRequest = jumpBuffer; }

			void Slide() { slide = true; }

			void SetInputVelocity(const vec3& velocity) { inputVelocity = velocity; }

			vec3 GetInputVelocity() const { return inputVelocity; }

            float slowVelocity = 1.6f;
            float fastVelocity = 4.0f;
            float jumpVelocity = 4.0f;

            // On the ground the velocity follows the input at these rates (m/s²): the acceleration speeds up along the
            // current direction, the deceleration slows down and turns. Zero follows the input at once
            float acceleration = 0.0f;
            float deceleration = 0.0f;

            // A jump pressed this long before landing happens on landing, one pressed this long after walking off an
            // edge still happens (seconds)
            float jumpBuffer = 0.15f;
            float coyoteTime = 0.12f;

			bool allowInput = true;
			
			float stickToGroundDistance = 0.1f;
            float walkStairStepUpDistance = 0.2f;

			float slideDeacceleration = 4.0f;
			float slideCutoffVelocity = 0.2f;
			float slideVelocityMax = 4.0f;

		private:
			void Update(float deltaTime);

			vec3 FollowInput(const vec3& groundVelocity, const vec3& up, float deltaTime) const;

			void InsertIntoPhysicsWorld(const TransformComponent& transformComponent,
				Physics::PhysicsWorld* physicsWorld);

			void RemoveFromPhysicsWorld();

			float jumpRequest = 0.0f;
			float airTime = 0.0f;
			bool slide = false;
			bool jumped = false;

			vec3 inputVelocity = vec3(0.0f);

			vec3 lastGravityAcceleration = vec3(0.0f);

			friend Scene;

		};

	}

}