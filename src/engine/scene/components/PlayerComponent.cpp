#include "PlayerComponent.h"
#include "physics/MathConversion.h"
#include "physics/PhysicsManager.h"

namespace Atlas::Scene::Components {

    PlayerComponent::PlayerComponent(float mass, float maxStrength) {

        creationSettings = CreateRef<Physics::PlayerCreationSettings>();
        creationSettings->mass = mass;
        creationSettings->maxStrength = maxStrength;
        creationSettings->shape = Physics::ShapesManager::CreateShape(Physics::CapsuleShapeSettings {});

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

        if (!jumped)
            StickToGround(-up * stickToGroundDistance);

        auto newVelocity = vec3(0.0f);
        auto groundVelocity = GetGroundVelocity();

        if (IsOnGround()) {
            jumped = false;
            newVelocity += groundVelocity;
            if (jump) {
                newVelocity += up * jumpVelocity;
                jumped = true;
            }
            if (allowInput && !slide)
                newVelocity += inputVelocity;

            if (slide) {
                // Get difference between ground velocity and actual velocty and dampen this over time
                vec3 slideVelocity = GetLinearVelocity() - groundVelocity;
                float slideVelocityLength = glm::min(glm::length(slideVelocity), slideVelocityMax);

                vec3 deaccVelocity = slideDeacceleration * 1.0f * slideVelocity * deltaTime;

                if (slideVelocityLength > slideCutoffVelocity && slideVelocityLength > glm::length(deaccVelocity)) {

                    if (glm::dot(slideVelocity, slideVelocity) > 0.0f) {
                        newVelocity += slideVelocity;
                        newVelocity -= deaccVelocity;
                    }
                }
                slide = false;
            }
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