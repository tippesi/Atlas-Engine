#include "RigidBodyComponent.h"

namespace Atlas {

    namespace Scene {

        namespace Components {

            Physics::BodyCreationSettings RigidBodyComponent::GetBodyCreationSettings() {

                if (bodyCreationSettings)
                    return *bodyCreationSettings;

                return Body::GetBodyCreationSettings();

            }

            void RigidBodyComponent::InsertIntoPhysicsWorld(const TransformComponent &transformComponent,
                Physics::PhysicsWorld* physicsWorld) {

                if (!bodyCreationSettings || !bodyCreationSettings->shape)
                    return;

                if (!bodyCreationSettings->shape->IsValid())
                    if (!bodyCreationSettings->shape->TryCreate())
                        return;

                this->world = physicsWorld;

                auto body = physicsWorld->CreateBody(*bodyCreationSettings, transformComponent.globalMatrix);

                // Just copy the body id, fine afterwards
                bodyId = body.bodyId;
                AE_ASSERT(!bodyId.IsInvalid() && "Body id is invalid");

                bodyCreationSettings = nullptr;

            }

            void RigidBodyComponent::RemoveFromPhysicsWorld() {

                if (!IsValid())
                    return;

                world->DestroyBody(*this);
                world = nullptr;

            }

        }

    }

}