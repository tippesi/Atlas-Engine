#include "PlayerComponent.h"
#include "physics/MathConversion.h"
#include "physics/PhysicsManager.h"

namespace Atlas::Scene::Components {

    void PlayerComponent::SetPosition(vec3 position) {

        AE_ASSERT(physicsWorld != nullptr && "Physics world is invalid");
        character->SetPosition(Physics::VecToJPHVec(position));

    }

    mat4 PlayerComponent::GetMatrix() const {

        AE_ASSERT(physicsWorld != nullptr && "Physics world is invalid");
        return Physics::JPHMatToMat(character->GetWorldTransform());

    }

    void PlayerComponent::SetLinearVelocity(vec3 velocity) {

        AE_ASSERT(physicsWorld != nullptr && "Physics world is invalid");
        character->SetLinearVelocity(Physics::VecToJPHVec(velocity));

    }

    vec3 PlayerComponent::GetLinearVelocity() const {

        AE_ASSERT(physicsWorld != nullptr && "Physics world is invalid");
        return Physics::JPHVecToVec(character->GetLinearVelocity());

    }

    vec3 PlayerComponent::GetGroundVelocity() const {

        AE_ASSERT(physicsWorld != nullptr && "Physics world is invalid");

        character->UpdateGroundVelocity();
        return Physics::JPHVecToVec(character->GetGroundVelocity());

    }

    bool PlayerComponent::IsOnGround() const {

        AE_ASSERT(physicsWorld != nullptr && "Physics world is invalid");
        return character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;

    }

    void PlayerComponent::SetUp(vec3 up) {

        AE_ASSERT(physicsWorld != nullptr && "Physics world is invalid");
        character->SetUp(Physics::VecToJPHVec(up));

    }

    vec3 PlayerComponent::GetUp() const {

        AE_ASSERT(physicsWorld != nullptr && "Physics world is invalid");
        return Physics::JPHVecToVec(character->GetUp());

    }

    void PlayerComponent::Update(const TransformComponent& transformComponent, float deltaTime) {

        auto& system = physicsWorld->system;

        if (physicsWorld->pauseSimulation)
            return;

        auto gravityVector = -GetUp() * glm::length(physicsWorld->GetGravity());
        character->Update(deltaTime, Physics::VecToJPHVec(gravityVector),
            system->GetDefaultBroadPhaseLayerFilter(Physics::Layers::MOVABLE), 
            system->GetDefaultLayerFilter(Physics::Layers::MOVABLE),
            {}, {}, *Physics::PhysicsManager::tempAllocator);

    }

    void PlayerComponent::InsertIntoPhysicsWorld(const TransformComponent& transformComponent,
        Physics::PhysicsWorld* physicsWorld) {

        if (!playerCreationSettings || !playerCreationSettings->shape)
            return;

        if (!playerCreationSettings->shape->IsValid())
            if (!playerCreationSettings->shape->TryCreate())
                return;

        this->physicsWorld = physicsWorld;

        auto translation = Physics::VecToJPHVec(transformComponent.Decompose().translation);

        JPH::Ref<JPH::CharacterVirtualSettings> settings = new JPH::CharacterVirtualSettings(playerCreationSettings->GetSettings());
        character = CreateRef<JPH::CharacterVirtual>(settings, translation,
            JPH::Quat::sIdentity(), physicsWorld->system.get());

    }

    void PlayerComponent::RemoveFromPhysicsWorld() {

        if (!Valid())
            return;

        physicsWorld = nullptr;

    }

}