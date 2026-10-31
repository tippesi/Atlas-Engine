#include "Body.h"
#include "PhysicsWorld.h"

namespace Atlas::Physics {

    void Body::SetMatrix(glm::mat4 matrix) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        world->SetBodyMatrix(bodyId, matrix);

    }

    void Body::MoveKinematic(glm::mat4 matrix, float deltaTime) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        world->MoveKinematicBody(bodyId, matrix, deltaTime);

    }

    mat4 Body::GetMatrix() {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        return world->GetBodyMatrix(bodyId);

    }

    void Body::SetMotionQuality(Physics::MotionQuality quality) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        world->SetMotionQuality(bodyId, quality);

    }

    Physics::MotionQuality Body::GetMotionQuality() {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        return world->GetMotionQuality(bodyId);

    }

    void Body::SetLinearVelocity(glm::vec3 velocity) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        world->SetLinearVelocity(bodyId, velocity);

    }

    vec3 Body::GetLinearVelocity() {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        return world->GetLinearVelocity(bodyId);

    }

    void Body::SetAngularVelocity(glm::vec3 velocity) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        world->SetAngularVelocity(bodyId, velocity);

    }

    vec3 Body::GetAngularVelocity() {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        return world->GetAngularVelocity(bodyId);

    }

    void Body::SetRestitution(float restitution) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        world->SetRestitution(bodyId, restitution);

    }

    float Body::GetRestitution() {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        return world->GetRestitution(bodyId);

    }

    void Body::SetFriction(float friction) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        world->SetFriction(bodyId, friction);

    }

    float Body::GetFriction() {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        return world->GetFriction(bodyId);

    }

    void Body::AddForce(vec3 force) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        world->AddForce(bodyId, force);

    }

    void Body::AddForce(vec3 force, vec3 position) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        world->AddForce(bodyId, force, position);

    }

    void Body::AddImpulse(vec3 impulse) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        world->AddImpulse(bodyId, impulse);

    }

    void Body::AddImpulse(vec3 impulse, vec3 position) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        world->AddImpulse(bodyId, impulse, position);

    }

    void Body::AddTorque(vec3 torque) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        world->AddTorque(bodyId, torque);

    }

    void Body::AddAngularImpulse(vec3 angularImpulse) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        world->AddAngularImpulse(bodyId, angularImpulse);

    }

    Ref<Shape> Body::GetShape() const {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        return world->bodyToShapeMap[bodyId];

    }

    uint64_t Body::GetUserData() const {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        return world->GetUserData(bodyId);

    }

    Physics::BodyCreationSettings Body::GetBodyCreationSettings() const {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        return world->GetBodyCreationSettings(bodyId);

    }

}
