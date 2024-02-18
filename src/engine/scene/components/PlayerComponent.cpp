#include "PlayerComponent.h"
#include "physics/MathConversion.h"
#include "physics/PhysicsManager.h"

namespace Atlas::Scene::Components {

    PlayerComponent::PlayerComponent(float mass, float maxStrength) {

        creationSettings = CreateRef<Physics::PlayerCreationSettings>();
        creationSettings->mass = mass;
        creationSettings->maxStrength = maxStrength;

    }

    PlayerComponent::PlayerComponent(const Physics::PlayerCreationSettings &playerCreationSettings) {

        creationSettings = CreateRef(playerCreationSettings);

    }

    void PlayerComponent::Update(float deltaTime) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");

        const auto& system = world->system;

        if (world->pauseSimulation)
            return;
        
        auto up = GetUp();

        auto newVelocity = vec3(0.0f);
        auto groundVelocity = GetGroundVelocity();
        if (IsOnGround()) {
            newVelocity += groundVelocity;
            if (jump)
                newVelocity += up * jumpVelocity;
            if (allowInput)
                newVelocity += inputVelocity;
        }
        else {
            newVelocity += GetLinearVelocity();
            // Add reduced input velocity such that jumping doesn't feel weird
            if (allowInput)
                newVelocity += inputVelocity * 0.01f;
        }

        jump = false;

        newVelocity += up * world->GetGravity() * deltaTime;

        SetLinearVelocity(newVelocity);

        Player::Update(deltaTime);
        
    }

    void PlayerComponent::InsertIntoPhysicsWorld(const TransformComponent& transformComponent,
        Physics::PhysicsWorld* physicsWorld) {

        if (!creationSettings || !creationSettings->shape)
            return;

        if (!creationSettings->shape->IsValid())
            if (!creationSettings->shape->TryCreate())
                return;

        auto decomp = transformComponent.Decompose();
        
        Init(physicsWorld, decomp.translation, quat());

    }

    void PlayerComponent::RemoveFromPhysicsWorld() {

        if (!IsValid())
            return;

        world = nullptr;

    }

}