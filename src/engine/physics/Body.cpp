#include "Body.h"
#include "PhysicsWorld.h"

namespace Atlas::Physics {

    void Body::SetMatrix(glm::mat4 matrix) {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        world->SetBodyMatrix(bodyId, matrix);

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

    uint64_t Body::GetUserData() const {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        return world->GetUserData(bodyId);

    }

    Physics::BodyCreationSettings Body::GetBodyCreationSettings() const {

        AE_ASSERT(world != nullptr && "Physics world is invalid");
        return world->GetBodyCreationSettings(bodyId);

    }

}
