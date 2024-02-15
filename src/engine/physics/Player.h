#pragma once

#include "Shape.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

namespace Atlas::Physics {

    class PhysicsWorld;

	struct PlayerCreationSettings {
		JPH::CharacterVirtualSettings GetSettings() const;

		float maxSlopeAngle = 0.87f;
		vec3 up = vec3(0.0f, 1.0f, 0.0f);

		float mass = 75.0f;
		float maxStrength = 100.0f;

        float predictiveContactDistance = 0.1f;
        float padding = 0.02f;

        vec3 shapeOffset;

		Ref<Shape> shape;
	};

    class Player {

    public:
        Player(const PlayerCreationSettings& creationSettings, const Ref<PhysicsWorld>& physicsWorld);

		bool IsValid() const { return world != nullptr; }

		void SetPosition(vec3 position);

		vec3 GetPosition() const;

		void SetRotation(quat rotation);

		quat GetRotation() const;

		mat4 GetMatrix() const;

		void SetLinearVelocity(vec3 velocity);

		vec3 GetLinearVelocity() const;

		vec3 GetGroundVelocity() const;

		bool IsOnGround() const;

		void SetUp(vec3 up);

		vec3 GetUp() const;

		void SetShape(const Ref<Shape>& shape);

		virtual void Update(float deltaTime);

		Ref<Physics::PlayerCreationSettings> playerCreationSettings = nullptr;

	protected:
		Ref<JPH::CharacterVirtual> character = nullptr;

		PhysicsWorld* world = nullptr;

    };

}