#include "RigidBodyComponent.h"

namespace Atlas {

    namespace Scene {

        namespace Components {

            void RigidBodyComponent::SetMatrix(glm::mat4 matrix) {

                assert(physicsWorld != nullptr && "Physics world is invalid");

                physicsWorld->SetBodyMatrix(bodyId, matrix);

            }

            mat4 RigidBodyComponent::GetMatrix() {

                assert(physicsWorld != nullptr && "Physics world is invalid");

                return physicsWorld->GetBodyMatrix(bodyId);

            }

            void RigidBodyComponent::SetLinearVelocity(glm::vec3 velocity) {

                assert(physicsWorld != nullptr && "Physics world is invalid");

                physicsWorld->SetLinearVelocity(bodyId, velocity);

            }

            vec3 RigidBodyComponent::GetLinearVelocity() {

                assert(physicsWorld != nullptr && "Physics world is invalid");

                return physicsWorld->GetLinearVelocity(bodyId);

            }

            void RigidBodyComponent::InsertIntoPhysicsWorld(const TransformComponent &transformComponent,
                Physics::PhysicsWorld* physicsWorld) {

                if (!shape || Valid())
                    return;

                this->physicsWorld = physicsWorld;

                bodyId = physicsWorld->CreateBody(shape, layer, transformComponent.globalMatrix);
                assert(!bodyId.IsInvalid() && "Body id is invalid");

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