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
                explicit RigidBodyComponent(const Ref<Physics::Shape> shape, JPH::ObjectLayer layer)
                    : shape(shape), layer(layer) {}

                inline const bool Valid() const { return physicsWorld != nullptr; }

                void SetMatrix(mat4 matrix);

                mat4 GetMatrix();

                Ref<Physics::Shape> shape = nullptr;
                JPH::ObjectLayer layer;

            private:
                void TryInsertIntoPhysicsWorld(const TransformComponent& transformComponent,
                    Physics::PhysicsWorld* physicsWorld);

                JPH::BodyID bodyId;
                Physics::PhysicsWorld* physicsWorld = nullptr;

                friend Scene;

            };

        }

    }

}