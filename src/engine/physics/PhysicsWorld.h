#pragma once

#include "../System.h"
#include "Shapes.h"

#include "InterfaceImplementations.h"

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

            void Update(float deltaTime);

            JPH::BodyID CreateBody(const Ref<Shape>& shape, JPH::ObjectLayer objectLayer, const mat4& matrix,
                vec3 velocity);

            void SetBodyMatrix(JPH::BodyID bodyId, const mat4& matrix);

            mat4 GetBodyMatrix(JPH::BodyID bodyId);

            void SetLinearVelocity(JPH::BodyID bodyId, vec3 velocity);

            vec3 GetLinearVelocity(JPH::BodyID bodyId);

            JPH::PhysicsSystem system;

            int32_t simulationStepsPerSecond = 120;

        private:
            void MatrixToJPHPosAndRot(const mat4& matrix, JPH::Vec3& pos, JPH::Quat& quat);
            void JPHPosAndRotToMatrix(const JPH::Vec3& pos, const JPH::Quat& quat, mat4& matrix);

            JPH::Vec3 VecToJPHVec(const vec3 vec);
            vec3 JPHVecToVec(const JPH::Vec3 vec);

            Ref<JPH::ObjectLayerPairFilter> objectLayerFilter = nullptr;
            Ref<JPH::BroadPhaseLayerInterface> broadPhaseLayerInterface = nullptr;
            Ref<JPH::ObjectVsBroadPhaseLayerFilter> objectVsBroadPhaseLayerFilter = nullptr;

            friend class Scene::Scene;
            friend class Scene::Components::RigidBodyComponent;

        };


    }

}