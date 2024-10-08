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
                    : layer(bodyCreationSettings.objectLayer), creationSettings(CreateRef(bodyCreationSettings)), entity(entity) {}

                Physics::BodyCreationSettings GetBodyCreationSettings() const override;

                Physics::ObjectLayer layer = Physics::Layers::Static;
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