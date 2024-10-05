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

		void SetUp(vec3 up);

		vec3 GetUp() const;

		void SetShape(const Ref<Shape>& shape);

		virtual void Update(float deltaTime);

		Ref<PlayerCreationSettings> creationSettings = nullptr;

        vec3 stickToGroundDist = vec3(0.0f);
        vec3 walkStairsStepUpDist = vec3(0.0f);

	protected:
		void Init(PhysicsWorld* world, vec3 initialPosition, quat initialRotation);

		virtual void OnContactAdded(const JPH::CharacterVirtual *inCharacter, const JPH::BodyID &inBodyID2, const JPH::SubShapeID &inSubShapeID2, 
			JPH::RVec3Arg inContactPosition, JPH::Vec3Arg inContactNormal, JPH::CharacterContactSettings &ioSettings) override;

		Ref<JPH::CharacterVirtual> character = nullptr;

		PhysicsWorld* world = nullptr;

    };

}