#include "RigidBodyComponent.h"

namespace Atlas {

    namespace Scene {

        namespace Components {

            RigidBodyComponent::RigidBodyComponent(Scene* scene, Entity entity, const RigidBodyComponent& that) {

                if (this != &that) {
                    *this = that;
                }

                this->entity = entity;

            }

            Physics::BodyCreationSettings RigidBodyComponent::GetBodyCreationSettings() {

                if (creationSettings)
                    return *creationSettings;

                return Body::GetBodyCreationSettings();

            }

            void RigidBodyComponent::InsertIntoPhysicsWorld(const TransformComponent &transformComponent,
                Physics::PhysicsWorld* physicsWorld) {

                if (!creationSettings || !creationSettings->shape)
                    return;

                if (!creationSettings->shape->IsValid())
                    if (!creationSettings->shape->TryCreate())
                        return;

                this->world = physicsWorld;

                auto body = physicsWorld->CreateBody(*creationSettings, transformComponent.globalMatrix, entity);

                // Just copy the body id, fine afterwards
                bodyId = body.bodyId;
                AE_ASSERT(!bodyId.IsInvalid() && "Body id is invalid");

                creationSettings = nullptr;

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