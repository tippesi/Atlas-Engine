#include "Player.h"
#include "MathConversion.h"
#include "PhysicsWorld.h"
#include "PhysicsManager.h"

namespace Atlas::Physics {

	JPH::CharacterVirtualSettings PlayerCreationSettings::GetSettings() const {

		JPH::CharacterVirtualSettings settings;

		settings.mMaxSlopeAngle = maxSlopeAngle;
		settings.mUp = VecToJPHVec(up);

		settings.mMass = mass;
		settings.mMaxStrength = maxStrength;

        settings.mPredictiveContactDistance = predictiveContactDistance;
        settings.mCharacterPadding = padding;

        settings.mShapeOffset = VecToJPHVec(shapeOffset);
		settings.mShape = shape->ref;
		
		return settings;

	}

    void Player::SetPosition(vec3 position) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        character->SetPosition(Physics::VecToJPHVec(position));

    }

    mat4 Player::GetMatrix() const {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        return Physics::JPHMatToMat(character->GetWorldTransform());

    }

    void Player::SetLinearVelocity(vec3 velocity) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        character->SetLinearVelocity(Physics::VecToJPHVec(velocity));

    }

    vec3 Player::GetLinearVelocity() const {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        return Physics::JPHVecToVec(character->GetLinearVelocity());

    }

    vec3 Player::GetGroundVelocity() const {

        AE_ASSERT(world != nullptr && "Physics world is invalid");

        character->UpdateGroundVelocity();
        return Physics::JPHVecToVec(character->GetGroundVelocity());

    }

    bool Player::IsOnGround() const {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        return character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;

    }

    void Player::SetUp(vec3 up) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        character->SetUp(Physics::VecToJPHVec(up));

    }

    vec3 Player::GetUp() const {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        return Physics::JPHVecToVec(character->GetUp());

    }

    void Player::SetShape(const Ref<Shape>& shape) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");

        auto system = world->system;

        const auto& physicsSettings = system->GetPhysicsSettings();
        const auto& broadPhaseLayerFilter = system->GetDefaultBroadPhaseLayerFilter(Layers::MOVABLE);
        const auto& defaultLayerFilter = system->GetDefaultLayerFilter(Physics::Layers::MOVABLE);
        const auto& tempAllocator = Physics::PhysicsManager::tempAllocator;

        character->SetShape(shape->ref, physicsSettings.mPenetrationSlop * 1.5f, broadPhaseLayerFilter,
            defaultLayerFilter, {}, {}, *tempAllocator);

    }

    void Player::Update(float deltaTime) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");

        auto system = world->system;

        const auto& physicsSettings = system->GetPhysicsSettings();
        const auto& broadPhaseLayerFilter = system->GetDefaultBroadPhaseLayerFilter(Layers::MOVABLE);
        const auto& defaultLayerFilter = system->GetDefaultLayerFilter(Physics::Layers::MOVABLE);
        const auto& tempAllocator = Physics::PhysicsManager::tempAllocator;

        auto gravityVector = -GetUp() * glm::length(world->GetGravity());

        character->Update(deltaTime, VecToJPHVec(gravityVector), broadPhaseLayerFilter,
            defaultLayerFilter, {}, {}, *tempAllocator);

    }

}