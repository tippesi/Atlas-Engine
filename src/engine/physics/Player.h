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

        float shapePadding = 0.02f;
        vec3 shapeOffset = vec3(0.0f);

        // Pushes bodies with up to the max strength, otherwise they block the player like static bodies
        bool canPushBodies = true;

		Ref<Shape> shape;
	};

    class Player : JPH::CharacterContactListener {

    public:
        Player() = default;

        Player(const PlayerCreationSettings& creationSettings, const vec3& initialPosition,
            const Ref<PhysicsWorld>& physicsWorld);

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

		// Sets the user data of the character and its inner body, e.g. the owning entity
		void SetUserData(uint64_t userData);

		void SetUp(vec3 up);

		vec3 GetUp() const;

		// Fails without changing the shape if the new shape would collide
		bool SetShape(const Ref<Shape>& shape);

		virtual void Update(float deltaTime);

		Ref<PlayerCreationSettings> creationSettings = nullptr;

        vec3 stickToGroundDist = vec3(0.0f);
        vec3 walkStairsStepUpDist = vec3(0.0f);

	protected:
		void Init(PhysicsWorld* world, vec3 initialPosition, quat initialRotation);

		virtual void OnContactAdded(const JPH::CharacterVirtual *inCharacter, const JPH::CharacterContact &inContact,
			JPH::CharacterContactSettings &ioSettings) override;

		Ref<JPH::CharacterVirtual> character = nullptr;

		PhysicsWorld* world = nullptr;

    };

}