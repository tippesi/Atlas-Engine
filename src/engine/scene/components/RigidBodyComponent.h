#pragma once

#include "../../System.h"
#include "../../physics/PhysicsWorld.h"

#include "TransformComponent.h"

namespace Atlas {

    namespace Scene {

        class Scene;

        namespace Components {

            class RigidBodyComponent : public Physics::Body {

            public:
                RigidBodyComponent() = default;
                RigidBodyComponent(const RigidBodyComponent& that) = default;
                explicit RigidBodyComponent(const Physics::BodyCreationSettings& bodyCreationSettings)
                    : layer(bodyCreationSettings.objectLayer), bodyCreationSettings(CreateRef(bodyCreationSettings)) {}

                Physics::BodyCreationSettings GetBodyCreationSettings();

                Physics::ObjectLayer layer = Physics::Layers::STATIC;
                Ref<Physics::BodyCreationSettings> bodyCreationSettings = nullptr;

            private:
                void InsertIntoPhysicsWorld(const TransformComponent& transformComponent,
                    Physics::PhysicsWorld* physicsWorld);

                void RemoveFromPhysicsWorld();

                friend Scene;

            };

        }

    }

}