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
                explicit RigidBodyComponent(const Physics::ShapeRef shape, JPH::ObjectLayer layer)
                    : shape(shape), layer(layer) {}

                inline const bool Valid() const { return physicsWorld != nullptr; }

                void SetMatrix(mat4 matrix);

                mat4 GetMatrix();

                void SetLinearVelocity(vec3 velocity);

                vec3 GetLinearVelocity();

                Physics::ShapeRef shape = nullptr;
                JPH::ObjectLayer layer;

            private:
                void InsertIntoPhysicsWorld(const TransformComponent& transformComponent,
                    Physics::PhysicsWorld* physicsWorld);

                void RemoveFromPhysicsWorld();

                Physics::Body bodyId;
                Physics::PhysicsWorld* physicsWorld = nullptr;

                friend Scene;

            };

        }

    }

}