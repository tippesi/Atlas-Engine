#pragma once

#include "../../System.h"
#include "../../physics/PhysicsWorld.h"
#include "../../physics/Player.h"

#include "TransformComponent.h"

namespace Atlas::Scene {

	class Scene;

	namespace Components {

		class PlayerComponent {

		public:
			PlayerComponent() = default;
			PlayerComponent(const PlayerComponent& that) = default;
			explicit PlayerComponent(const Physics::PlayerCreationSettings& playerCreationSettings)
				: playerCreationSettings(CreateRef(playerCreationSettings)) {}

			inline const bool Valid() const { return physicsWorld != nullptr; }

			void SetPosition(vec3 position);

			vec3 GetPosition() const;

			mat4 GetMatrix() const;

			void SetLinearVelocity(vec3 velocity);

			vec3 GetLinearVelocity() const;

			vec3 GetGroundVelocity() const;

			bool IsOnGround() const;

			void SetUp(vec3 up);

			vec3 GetUp() const;

			Ref<Physics::PlayerCreationSettings> playerCreationSettings = nullptr;

		private:
			void Update(const TransformComponent& transformComponent, float deltaTime);

			void InsertIntoPhysicsWorld(const TransformComponent& transformComponent,
				Physics::PhysicsWorld* physicsWorld);

			void RemoveFromPhysicsWorld();

			Ref<JPH::CharacterVirtual> character = nullptr;

			Physics::PhysicsWorld* physicsWorld = nullptr;

			friend Scene;

		};

	}

}