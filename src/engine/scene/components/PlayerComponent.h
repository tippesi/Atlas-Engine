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

			void Jump() { jump = true; }

			void Slide() { slide = true; }

			void SetInputVelocity(const vec3& velocity) { inputVelocity = velocity; }

			vec3 GetInputVelocity() const { return inputVelocity; }

            float slowVelocity = 1.6f;
            float fastVelocity = 4.0f;
            float jumpVelocity = 4.0f;

			bool allowInput = true;
			
			float stickToGroundDistance = 0.1f;

			float slideDeacceleration = 4.0f;
			float slideCutoffVelocity = 0.2f;
			float slideVelocityMax = 4.0f;

		private:
			void Update(float deltaTime);

			void InsertIntoPhysicsWorld(const TransformComponent& transformComponent,
				Physics::PhysicsWorld* physicsWorld);

			void RemoveFromPhysicsWorld();

			bool jump = false;
			bool slide = false;
			bool jumped = false;

			vec3 inputVelocity = vec3(0.0f);

			vec3 lastGravityAcceleration = vec3(0.0f);

			friend Scene;

		};

	}

}