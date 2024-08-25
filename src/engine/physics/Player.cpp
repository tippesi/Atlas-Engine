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

        settings.mMaxCollisionIterations = 10;
        settings.mMaxConstraintIterations = 10;
        settings.mShapeOffset = VecToJPHVec(shapeOffset);
		settings.mShape = shape->ref;
        settings.mInnerBodyShape = shape->ref;
        settings.mInnerBodyLayer = Layers::Movable;

        if (shape->type == ShapeType::Capsule) {
            auto shapeSettings = static_cast<CapsuleShapeSettings*>(shape->settings.get());
            settings.mSupportingVolume = { JPH::Vec3::sAxisY(), -shapeSettings->radius };
        }
		
		return settings;

	}

    Player::Player(const Atlas::Physics::PlayerCreationSettings &creationSettings, const glm::vec3 &initialPosition,
        const Ref<Atlas::Physics::PhysicsWorld> &physicsWorld) : creationSettings(CreateRef(creationSettings)) {

        Init(physicsWorld.get(), initialPosition, quat());

    }

    void Player::SetPosition(vec3 position) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        character->SetPosition(VecToJPHVec(position));

    }

    vec3 Player::GetPosition() const {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        return JPHVecToVec(character->GetPosition());

    }

    void Player::SetRotation(quat rotation) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        character->SetRotation(QuatToJPHQuat(rotation));

    }

    quat Player::GetRotation() const {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        return JPHQuatToQuat(character->GetRotation());

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
        const auto& broadPhaseLayerFilter = system->GetDefaultBroadPhaseLayerFilter(Layers::Movable);
        const auto& defaultLayerFilter = system->GetDefaultLayerFilter(Physics::Layers::Movable);
        const auto& tempAllocator = Physics::PhysicsManager::tempAllocator;

        character->SetShape(shape->ref, physicsSettings.mPenetrationSlop * 1.5f, broadPhaseLayerFilter,
            defaultLayerFilter, {}, {}, *tempAllocator);

    }

    void Player::Update(float deltaTime) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");

        auto system = world->system;

        const auto& physicsSettings = system->GetPhysicsSettings();
        const auto& broadPhaseLayerFilter = system->GetDefaultBroadPhaseLayerFilter(Layers::Movable);
        const auto& defaultLayerFilter = system->GetDefaultLayerFilter(Physics::Layers::Movable);
        const auto& tempAllocator = Physics::PhysicsManager::tempAllocator;

        auto gravityVector = -GetUp() * glm::length(world->GetGravity());

        character->SetEnhancedInternalEdgeRemoval(true);

        JPH::CharacterVirtual::ExtendedUpdateSettings settings;

        settings.mStickToFloorStepDown = VecToJPHVec(stickToGroundDist);
        settings.mWalkStairsStepUp = VecToJPHVec(walkStairsStepUpDist);

        character->ExtendedUpdate(deltaTime, VecToJPHVec(gravityVector), settings,
            broadPhaseLayerFilter, defaultLayerFilter, {}, {}, *tempAllocator);

    }

    void Player::Init(PhysicsWorld* world, vec3 initialPosition, quat initialRotation) {

        this->world = world;

        JPH::Ref<JPH::CharacterVirtualSettings> settings = 
            new JPH::CharacterVirtualSettings(creationSettings->GetSettings());

        character = CreateRef<JPH::CharacterVirtual>(settings, VecToJPHVec(initialPosition),
            QuatToJPHQuat(initialRotation), world->system.get());

        character->SetListener(this);

    }

    void Player::OnContactAdded(const JPH::CharacterVirtual *inCharacter, const JPH::BodyID &inBodyID2, const JPH::SubShapeID &inSubShapeID2, 
			JPH::RVec3Arg inContactPosition, JPH::Vec3Arg inContactNormal, JPH::CharacterContactSettings &ioSettings) {

        ioSettings.mCanPushCharacter = false;

    }

}