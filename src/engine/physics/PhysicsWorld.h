#pragma once

#include "../System.h"
#include "../volume/Ray.h"
#include "ShapesManager.h"
#include "BodyCreation.h"
#include "Body.h"
#include "Player.h"

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

        class PhysicsWorld {

        public:
            PhysicsWorld(uint32_t maxBodyCount = 65536, uint32_t bodyMutexesCount = 0, uint32_t maxBodyPairCount= 65536,
                uint32_t maxContactConstraintCount = 65536, Ref<JPH::ObjectLayerPairFilter> objectLayerFilter = nullptr,
                Ref<JPH::BroadPhaseLayerInterface> broadPhaseLayerInterface = nullptr,
                Ref<JPH::ObjectVsBroadPhaseLayerFilter> objectVsBroadPhaseLayerFilter = nullptr);

            ~PhysicsWorld();

            void Update(float deltaTime);

            Body CreateBody(const BodyCreationSettings& bodyCreationSettings, const mat4& matrix, uint64_t userData = 0);

            void DestroyBody(Body body);

            void SetBodyMatrix(BodyID bodyId, const mat4& matrix);

            mat4 GetBodyMatrix(BodyID bodyId);

            void SetMotionQuality(BodyID bodyId, MotionQuality quality);

            MotionQuality GetMotionQuality(BodyID bodyId);

            void SetLinearVelocity(BodyID bodyId, vec3 velocity);

            vec3 GetLinearVelocity(BodyID bodyId);

            void SetRestitution(BodyID bodyId, float restitution);

            float GetRestitution(BodyID bodyId);

            void SetFriction(BodyID bodyId, float friction);

            float GetFriction(BodyID bodyId);

            uint64_t GetUserData(BodyID bodyId);

            void ChangeShape(BodyID bodyId, Ref<Shape> shape);

            BodyCreationSettings GetBodyCreationSettings(BodyID bodyId);

            void SetGravity(vec3 gravity);

            vec3 GetGravity();

            Volume::RayResult<Body> CastRay(Volume::Ray& ray);

            void OptimizeBroadphase();

            void SaveState();

            void RestoreState();

            Ref<JPH::PhysicsSystem> system = nullptr;

            int32_t simulationStepsPerSecond = 60;
            bool pauseSimulation = false;

            std::unordered_map<BodyID, Ref<Shape>> bodyToShapeMap;

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