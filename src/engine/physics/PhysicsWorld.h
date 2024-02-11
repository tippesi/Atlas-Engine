#pragma once

#include "../System.h"
#include "ShapesManager.h"
#include "BodyCreation.h"

#include "InterfaceImplementations.h"
#include <Jolt/Physics/StateRecorderImpl.h>

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

            ~PhysicsWorld();

            void Update(float deltaTime);

            Body CreateBody(const BodyCreationSettings& bodyCreationSettings, const mat4& matrix);

            void DestroyBody(Body bodyId);

            void SetBodyMatrix(Body bodyId, const mat4& matrix);

            mat4 GetBodyMatrix(Body bodyId);

            void SetMotionQuality(Body bodyId, MotionQuality quality);

            MotionQuality GetMotionQuality(Body bodyId);

            void SetLinearVelocity(Body bodyId, vec3 velocity);

            vec3 GetLinearVelocity(Body bodyId);

            void SetRestitution(Body bodyId, float restitution);

            float GetRestitution(Body bodyId);

            void SetFriction(Body bodyId, float friction);

            float GetFriction(Body bodyId);

            void ChangeShape(Body bodyId, Ref<Shape> shape);

            BodyCreationSettings GetBodyCreationSettings(Body bodyId);

            void OptimizeBroadphase();

            void SaveState();

            void RestoreState();

            Ref<JPH::PhysicsSystem> system = nullptr;

            int32_t simulationStepsPerSecond = 60;
            bool pauseSimulation = false;

            std::unordered_map<Body, Ref<Shape>> bodyToShapeMap;

        private:
            Ref<JPH::ObjectLayerPairFilter> objectLayerFilter = nullptr;
            Ref<JPH::BroadPhaseLayerInterface> broadPhaseLayerInterface = nullptr;
            Ref<JPH::ObjectVsBroadPhaseLayerFilter> objectVsBroadPhaseLayerFilter = nullptr;
            Ref<JPH::ContactListener> contactListener = nullptr;

            Ref<JPH::StateRecorderImpl> state = nullptr;

            friend class Scene::Scene;
            friend class RigidBodyComponent;
            friend class PhysicsSerializer;

        };


    }

}