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

        // Don't want to stick to the ground while jumping
        if (!jumped)
            stickToGroundDist = -up * stickToGroundDistance;
        else
            stickToGroundDist = vec3(0.0f);

        auto newVelocity = vec3(0.0f);
        auto groundVelocity = GetGroundVelocity();

        if (IsOnGround()) {
            jumped = false;
            airTime = 0.0f;
            newVelocity += groundVelocity;
            if (jumpRequest > 0.0f) {
                newVelocity += up * jumpVelocity;
                jumped = true;
                jumpRequest = 0.0f;
            }
            if (allowInput && !slide)
                newVelocity += FollowInput(groundVelocity, up, deltaTime);

            if (slide) {
                // Get difference between ground velocity and actual velocty and dampen this over time
                vec3 slideVelocity = GetLinearVelocity() - groundVelocity;
                // Cancel out vertical velocity, we are on the ground
                slideVelocity -= lastGravityAcceleration;
                float slideVelocityLength = glm::min(glm::length(slideVelocity), slideVelocityMax);

                vec3 deaccVelocity = slideDeacceleration * slideVelocity * deltaTime;

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
            // Just walked off an edge: the jump still counts, the fall so far is cancelled
            airTime += deltaTime;
            if (jumpRequest > 0.0f && !jumped && airTime <= coyoteTime) {
                newVelocity -= up * glm::dot(newVelocity, up);
                newVelocity += up * jumpVelocity;
                jumped = true;
                jumpRequest = 0.0f;
            }
        }

        jumpRequest = glm::max(0.0f, jumpRequest - deltaTime);

        lastGravityAcceleration = up * world->GetGravity() * deltaTime;
        newVelocity += lastGravityAcceleration;

        SetLinearVelocity(newVelocity);

        walkStairsStepUpDist = up * walkStairStepUpDistance;

        Player::Update(deltaTime);
        
    }

    vec3 PlayerComponent::FollowInput(const vec3& groundVelocity, const vec3& up, float deltaTime) const {

        // The horizontal velocity relative to the ground, as the character moved it (pushed or blocked)
        auto current = GetLinearVelocity() - groundVelocity;
        current -= up * glm::dot(current, up);

        auto difference = inputVelocity - current;
        auto distance = glm::length(difference);
        auto speedingUp = glm::dot(inputVelocity, current) >= 0.0f && glm::dot(inputVelocity, inputVelocity) >= glm::dot(current, current);
        auto rate = speedingUp ? acceleration : deceleration;
        if (rate <= 0.0f || distance <= rate * deltaTime)
            return inputVelocity;

        return current + difference / distance * rate * deltaTime;

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