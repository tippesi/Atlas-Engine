#pragma once

#include "../System.h"
#include "ShapesManager.h"

#include "InterfaceImplementations.h"

namespace Atlas {

    namespace Scene {

        class Scene;

        namespace Components {

            class RigidBodyComponent;

        }

    }

    namespace Physics {

        using Body = JPH::BodyID;

        class PhysicsWorld {

        public:
            PhysicsWorld(uint32_t maxBodyCount = 65536, uint32_t bodyMutexesCount = 0, uint32_t maxBodyPairCount= 65536,
                uint32_t maxContactConstraintCount = 65536, Ref<JPH::ObjectLayerPairFilter> objectLayerFilter = nullptr,
                Ref<JPH::BroadPhaseLayerInterface> broadPhaseLayerInterface = nullptr,
                Ref<JPH::ObjectVsBroadPhaseLayerFilter> objectVsBroadPhaseLayerFilter = nullptr);

            void Update(float deltaTime);

            Body CreateBody(const ShapeRef& shape, JPH::ObjectLayer objectLayer, const mat4& matrix,
                vec3 velocity = vec3(0.0f));

            void DestroyBody(Body bodyId);

            void SetBodyMatrix(Body bodyId, const mat4& matrix);

            mat4 GetBodyMatrix(Body bodyId);

            void SetLinearVelocity(Body bodyId, vec3 velocity);

            vec3 GetLinearVelocity(Body bodyId);

            void OptimizeBroadphase();

            JPH::PhysicsSystem system;

            int32_t simulationStepsPerSecond = 120;

        private:
            Ref<JPH::ObjectLayerPairFilter> objectLayerFilter = nullptr;
            Ref<JPH::BroadPhaseLayerInterface> broadPhaseLayerInterface = nullptr;
            Ref<JPH::ObjectVsBroadPhaseLayerFilter> objectVsBroadPhaseLayerFilter = nullptr;
            Ref<JPH::ContactListener> contactListener = nullptr;

            friend class Scene::Scene;
            friend class Scene::Components::RigidBodyComponent;

        };


    }

}