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
        
        UpdateCrouching();

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
            // Add reduced input velocity such that jumping doesn't feel weird. It steers, but doesn't make the
            // character faster than the input or the speed it left the ground with
            if (allowInput) {
                auto horizontal = newVelocity - up * glm::dot(newVelocity, up);
                auto steered = horizontal + inputVelocity * 0.01f;
                auto maxSpeed = glm::max(glm::length(horizontal), glm::length(inputVelocity));
                auto speed = glm::length(steered);
                if (speed > maxSpeed)
                    steered *= maxSpeed / speed;
                newVelocity += steered - horizontal;
            }
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

        UpdateStepOffset(up, deltaTime);
        
    }

    void PlayerComponent::UpdateStepOffset(const vec3& up, float deltaTime) {

        const float smoothingTime = 0.08f;
        const float maxOffset = 0.3f;

        stepOffset *= glm::exp(-deltaTime / smoothingTime);

        // The entity stays where it was before a step up a stair or down onto the floor and follows it from there
        stepOffset -= up * glm::dot(stepMovement, up);

        auto length = glm::length(stepOffset);
        if (length > maxOffset)
            stepOffset *= maxOffset / length;

    }

    void PlayerComponent::UpdateCrouching() {

        auto crouchRequested = crouch;
        crouch = false;

        auto& shape = creationSettings->shape;
        if (crouchRequested == crouching || shape->type != Physics::ShapeType::Capsule || !shape->settings)
            return;

        if (crouchRequested) {
            auto capsuleSettings = *static_cast<Physics::CapsuleShapeSettings*>(shape->settings.get());
            auto height = capsuleSettings.height;
            capsuleSettings.height = glm::min(crouchHeight, height);
            // The lower capsule keeps its bottom where the standing one has it, the feet stay on the ground
            Physics::CompoundShapeSettings crouchShapeSettings;
            crouchShapeSettings.children.push_back({ .shape = Physics::ShapesManager::CreateShape(capsuleSettings),
                .translation = vec3(0.0f, (capsuleSettings.height - height) * 0.5f * capsuleSettings.scale.y, 0.0f) });
            // The lower shape lies within the standing one, nothing can be in its way
            auto crouchShape = Physics::ShapesManager::CreateShape(crouchShapeSettings);
            if (crouchShape->IsValid() && SetShape(crouchShape, std::numeric_limits<float>::max()))
                crouching = true;
        }
        else if (SetShape(shape)) {
            crouching = false;
        }

    }

    vec3 PlayerComponent::FollowInput(const vec3& groundVelocity, const vec3& up, float deltaTime) const {

        // The horizontal velocity relative to the ground, as the character moved it (pushed or blocked)
        auto current = GetLinearVelocity() - groundVelocity;
        current -= up * glm::dot(current, up);

        auto target = inputVelocity;
        if (crouching && glm::length(target) > crouchVelocity)
            target = glm::normalize(target) * crouchVelocity;

        auto difference = target - current;
        auto distance = glm::length(difference);
        auto speedingUp = glm::dot(target, current) >= 0.0f && glm::dot(target, target) >= glm::dot(current, current);
        auto rate = speedingUp ? acceleration : deceleration;
        if (rate <= 0.0f || distance <= rate * deltaTime)
            return target;

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