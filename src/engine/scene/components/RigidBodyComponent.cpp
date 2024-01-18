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

            void RigidBodyComponent::TryInsertIntoPhysicsWorld(const TransformComponent &transformComponent,
                Physics::PhysicsWorld* physicsWorld, vec3 velocity) {

                if (!shape || !shape->TryCreateShape())
                    return;

                this->physicsWorld = physicsWorld;

                bodyId = physicsWorld->CreateBody(shape, layer, transformComponent.globalMatrix, velocity);
                assert(!bodyId.IsInvalid() && "Body id is invalid");

            }

        }

    }

}