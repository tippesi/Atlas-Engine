#pragma once

#include "../../System.h"
#include "../../physics/PhysicsWorld.h"

#include "TransformComponent.h"

namespace Atlas {

    namespace Scene {

        class Scene;

        namespace Components {

            class RigidBodyComponent {

            public:
                RigidBodyComponent() = default;
                RigidBodyComponent(const RigidBodyComponent& that) = default;
                explicit RigidBodyComponent(const Physics::BodyCreationSettings& bodyCreationSettings)
                    : layer(bodyCreationSettings.objectLayer), bodyCreationSettings(CreateRef(bodyCreationSettings)) {}

                inline const bool Valid() const { return physicsWorld != nullptr; }

                void SetMatrix(mat4 matrix);

                mat4 GetMatrix();

                void SetMotionQuality(Physics::MotionQuality quality);

                Physics::MotionQuality GetMotionQuality();

                void SetLinearVelocity(vec3 velocity);

                vec3 GetLinearVelocity();

                void SetRestitution(float restitution);

                float GetRestitution();

                void SetFriction(float friction);

                float GetFriction();

                Physics::BodyCreationSettings GetBodyCreationSettings();

                Physics::Body bodyId;
                Physics::ObjectLayer layer = Physics::Layers::STATIC;

                Ref<Physics::BodyCreationSettings> bodyCreationSettings = nullptr;

            private:
                void InsertIntoPhysicsWorld(const TransformComponent& transformComponent,
                    Physics::PhysicsWorld* physicsWorld);

                void RemoveFromPhysicsWorld();

                Physics::PhysicsWorld* physicsWorld = nullptr;

                friend Scene;

            };

        }

    }

}