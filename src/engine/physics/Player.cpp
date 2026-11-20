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
        settings.mCharacterPadding = shapePadding;

        if (shape->type == ShapeType::Capsule) {
            auto shapeSettings = static_cast<CapsuleShapeSettings*>(shape->settings.get());
            settings.mSupportingVolume = { JPH::Vec3::sAxisY(), -shapeSettings->radius };
        }
		
		return settings;

	}

    Player::Player(const Atlas::Physics::PlayerCreationSettings &creationSettings, const vec3 &initialPosition,
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

    bool Player::SetShape(const Ref<Shape>& shape, float maxPenetration) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");

        auto system = world->system;

        const auto& physicsSettings = system->GetPhysicsSettings();
        const auto& broadPhaseLayerFilter = system->GetDefaultBroadPhaseLayerFilter(Layers::Movable);
        const auto& defaultLayerFilter = system->GetDefaultLayerFilter(Physics::Layers::Movable);
        const auto& tempAllocator = Physics::PhysicsManager::tempAllocator;

        if (maxPenetration < 0.0f)
            maxPenetration = physicsSettings.mPenetrationSlop * 1.5f;
        if (!character->SetShape(shape->ref, maxPenetration, broadPhaseLayerFilter,
            defaultLayerFilter, {}, {}, *tempAllocator))
            return false;

        character->SetInnerBodyShape(shape->ref);

        return true;

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

        // The steps of CharacterVirtual::ExtendedUpdate, done here to know how far sticking to the floor and walking
        // up stairs moved the character (stepMovement)
        stepMovement = vec3(0.0f);
        character->StartTrackingContactChanges();

        auto up = character->GetUp();
        auto desiredVelocity = character->GetLinearVelocity();
        character->SetLinearVelocity(character->CancelVelocityTowardsSteepSlopes(desiredVelocity));

        auto oldPosition = character->GetPosition();
        auto groundToAir = character->IsSupported();

        character->Update(deltaTime, VecToJPHVec(gravityVector), broadPhaseLayerFilter, defaultLayerFilter, {}, {}, *tempAllocator);

        if (character->IsSupported())
            groundToAir = false;

        // Going from supported to not supported without moving up the character sticks to the floor
        if (groundToAir && !settings.mStickToFloorStepDown.IsNearZero()) {
            auto velocity = JPH::Vec3(character->GetPosition() - oldPosition).Dot(up) / deltaTime;
            if (velocity <= 1.0e-6f) {
                auto position = character->GetPosition();
                character->StickToFloor(settings.mStickToFloorStepDown, broadPhaseLayerFilter, defaultLayerFilter, {}, {}, *tempAllocator);

                // Only the drop that the slope of the floor doesn't explain is a step, walking down a slope isn't
                auto movement = JPH::Vec3(character->GetPosition() - oldPosition);
                auto horizontal = (movement - movement.Dot(up) * up).Length();
                auto cosSlope = glm::clamp(character->GetGroundNormal().Dot(up), 0.01f, 1.0f);
                auto slopeDrop = horizontal * glm::sqrt(1.0f - cosSlope * cosSlope) / cosSlope;
                auto drop = -JPH::Vec3(character->GetPosition() - position).Dot(up);
                if (drop > slopeDrop)
                    stepMovement -= JPHVecToVec(up) * (drop - slopeDrop);
            }
        }

        // Held back by a slope that is too steep the character tries to walk up a stair
        auto desiredStep = desiredVelocity * deltaTime;
        desiredStep -= desiredStep.Dot(up) * up;
        auto desiredStepLength = desiredStep.Length();
        if (!settings.mWalkStairsStepUp.IsNearZero() && desiredStepLength > 0.0f) {
            auto forward = desiredStep / desiredStepLength;
            auto achievedStep = JPH::Vec3(character->GetPosition() - oldPosition);
            auto achievedStepLength = glm::max(0.0f, achievedStep.Dot(forward));

            if (achievedStepLength + 1.0e-4f < desiredStepLength && character->CanWalkStairs(desiredVelocity)) {
                auto stepForward = forward * glm::max(settings.mWalkStairsMinStepForward, desiredStepLength - achievedStepLength);

                // Tests for the floor in the direction of the ground normal, unless it points away from the movement
                auto stepForwardTest = -character->GetGroundNormal();
                stepForwardTest -= stepForwardTest.Dot(up) * up;
                stepForwardTest = stepForwardTest.NormalizedOr(forward);
                if (stepForwardTest.Dot(forward) < settings.mWalkStairsCosAngleForwardContact)
                    stepForwardTest = forward;
                stepForwardTest *= settings.mWalkStairsStepForwardTest;

                auto position = character->GetPosition();
                character->WalkStairs(deltaTime, settings.mWalkStairsStepUp, stepForward, stepForwardTest,
                    settings.mWalkStairsStepDownExtra, broadPhaseLayerFilter, defaultLayerFilter, {}, {}, *tempAllocator);
                stepMovement += JPHVecToVec(JPH::Vec3(character->GetPosition() - position));
            }
        }

        character->FinishTrackingContactChanges();

    }

    void Player::Init(PhysicsWorld* world, vec3 initialPosition, quat initialRotation) {

        this->world = world;

        JPH::Ref<JPH::CharacterVirtualSettings> settings = 
            new JPH::CharacterVirtualSettings(creationSettings->GetSettings());

        character = CreateRef<JPH::CharacterVirtual>(settings, VecToJPHVec(initialPosition),
            QuatToJPHQuat(initialRotation), world->system.get());

        character->SetListener(this);

    }

    void Player::SetUserData(uint64_t userData) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        character->SetUserData(userData);

        auto innerBodyId = character->GetInnerBodyID();
        if (!innerBodyId.IsInvalid())
            world->system->GetBodyInterface().SetUserData(innerBodyId, userData);

    }

    void Player::OnContactAdded(const JPH::CharacterVirtual *inCharacter, const JPH::CharacterContact &inContact,
			JPH::CharacterContactSettings &ioSettings) {

        ioSettings.mCanPushCharacter = false;
        ioSettings.mCanReceiveImpulses = creationSettings->canPushBodies;

    }

}