#include "RigidBodyComponent.h"

namespace Atlas {

    namespace Scene {

        namespace Components {

            void RigidBodyComponent::SetMatrix(glm::mat4 matrix) {

                AE_ASSERT(physicsWorld != nullptr && "Physics world is invalid");
                physicsWorld->SetBodyMatrix(bodyId, matrix);

            }

            mat4 RigidBodyComponent::GetMatrix() {

                AE_ASSERT(physicsWorld != nullptr && "Physics world is invalid");
                return physicsWorld->GetBodyMatrix(bodyId);

            }

            void RigidBodyComponent::SetMotionQuality(Physics::MotionQuality quality) {

                AE_ASSERT(physicsWorld != nullptr && "Physics world is invalid");
                physicsWorld->SetMotionQuality(bodyId, quality);

            }

            Physics::MotionQuality RigidBodyComponent::GetMotionQuality() {

                AE_ASSERT(physicsWorld != nullptr && "Physics world is invalid");
                return physicsWorld->GetMotionQuality(bodyId);

            }

            void RigidBodyComponent::SetLinearVelocity(glm::vec3 velocity) {

                AE_ASSERT(physicsWorld != nullptr && "Physics world is invalid");
                physicsWorld->SetLinearVelocity(bodyId, velocity);

            }

            vec3 RigidBodyComponent::GetLinearVelocity() {

                AE_ASSERT(physicsWorld != nullptr && "Physics world is invalid");
                return physicsWorld->GetLinearVelocity(bodyId);

            }

            void RigidBodyComponent::SetRestitution(float restitution) {

                AE_ASSERT(physicsWorld != nullptr && "Physics world is invalid");
                physicsWorld->SetRestitution(bodyId, restitution);

            }

            float RigidBodyComponent::GetRestitution() {

                AE_ASSERT(physicsWorld != nullptr && "Physics world is invalid");
                return physicsWorld->GetRestitution(bodyId);

            }

            void RigidBodyComponent::SetFriction(float friction) {

                AE_ASSERT(physicsWorld != nullptr && "Physics world is invalid");
                physicsWorld->SetFriction(bodyId, friction);

            }

            float RigidBodyComponent::GetFriction() {

                AE_ASSERT(physicsWorld != nullptr && "Physics world is invalid");
                return physicsWorld->GetFriction(bodyId);

            }

            void RigidBodyComponent::InsertIntoPhysicsWorld(const TransformComponent &transformComponent,
                Physics::PhysicsWorld* physicsWorld) {

                if (!shape || Valid())
                    return;

                this->physicsWorld = physicsWorld;


                bodyId = physicsWorld->CreateBody(shape, layer, Physics::MotionQuality::Discrete,
                    transformComponent.globalMatrix);
                AE_ASSERT(!bodyId.IsInvalid() && "Body id is invalid");

            }

            void RigidBodyComponent::RemoveFromPhysicsWorld() {

                if (!Valid())
                    return;

                physicsWorld->DestroyBody(bodyId);
                physicsWorld = nullptr;

            }

        }

    }

}