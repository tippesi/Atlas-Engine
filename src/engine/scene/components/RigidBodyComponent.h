#pragma once

#include "../../System.h"
#include "../../physics/PhysicsWorld.h"

#include "../Entity.h"
#include "TransformComponent.h"

namespace Atlas {

    namespace Scene {

        class Scene;

        namespace Components {

            class RigidBodyComponent : public Physics::Body {

            public:
                RigidBodyComponent() = default;
                RigidBodyComponent(Scene* scene, Entity entity) : entity(entity) {}
                RigidBodyComponent(Scene* scene, Entity entity, const RigidBodyComponent& that);
                explicit RigidBodyComponent(Scene* scene, Entity entity, const Physics::BodyCreationSettings& bodyCreationSettings)
                    : entity(entity), layer(bodyCreationSettings.objectLayer), creationSettings(CreateRef(bodyCreationSettings)) {}

                Physics::BodyCreationSettings GetBodyCreationSettings() override;

                Physics::ObjectLayer layer = Physics::Layers::STATIC;
                Ref<Physics::BodyCreationSettings> creationSettings = nullptr;

            private:
                void InsertIntoPhysicsWorld(const TransformComponent& transformComponent,
                    Physics::PhysicsWorld* physicsWorld);

                void RemoveFromPhysicsWorld();

                ECS::Entity entity;

                friend Scene;

            };

        }

    }

}