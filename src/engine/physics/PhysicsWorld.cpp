#include "PhysicsWorld.h"
#include "PhysicsManager.h"

#include "../common/MatrixDecomposition.h"

#include <Jolt/Physics/Body/BodyCreationSettings.h>

namespace Atlas {

    namespace Physics {

        PhysicsWorld::PhysicsWorld(uint32_t maxBodyCount, uint32_t bodyMutexesCount, uint32_t maxBodyPairCount,
            uint32_t maxContactConstraintCount, Ref<JPH::ObjectLayerPairFilter> objectLayerFilter,
            Ref<JPH::BroadPhaseLayerInterface> broadPhaseLayerInterface,
            Ref<JPH::ObjectVsBroadPhaseLayerFilter> objectVsBroadPhaseLayerFilter) :
            objectLayerFilter(objectLayerFilter), broadPhaseLayerInterface(broadPhaseLayerInterface),
            objectVsBroadPhaseLayerFilter(objectVsBroadPhaseLayerFilter) {

            if (objectLayerFilter == nullptr)
                this->objectLayerFilter = Atlas::CreateRef<ObjectLayerPairFilterImpl>();
            if (broadPhaseLayerInterface == nullptr)
                this->broadPhaseLayerInterface = Atlas::CreateRef<BroadPhaseLayerInterfaceImpl>();
            if (objectVsBroadPhaseLayerFilter == nullptr)
                this->objectVsBroadPhaseLayerFilter = Atlas::CreateRef<ObjectVsBroadPhaseLayerFilterImpl>();

            system.Init(maxBodyCount, bodyMutexesCount, maxBodyPairCount, maxContactConstraintCount,
                *this->broadPhaseLayerInterface, *this->objectVsBroadPhaseLayerFilter, *this->objectLayerFilter);

        }

        void PhysicsWorld::Update(float deltaTime) {

            PhysicsManager::ExecuteUpdate(this, deltaTime);

        }

        JPH::BodyID PhysicsWorld::CreateBody(const Ref<Shape> &shape, JPH::ObjectLayer objectLayer,
            const mat4& matrix, vec3 veloctiy) {

            auto& bodyInterface = system.GetBodyInterface();

            JPH::Vec3 pos;
            JPH::Quat quat;
            MatrixToJPHPosAndRot(matrix, pos, quat);

            JPH::EMotionType motionType;
            switch(objectLayer) {
                case Layers::STATIC: motionType = JPH::EMotionType::Static; break;
                case Layers::MOVABLE: motionType = JPH::EMotionType::Dynamic; break;
                default: motionType = JPH::EMotionType::Static; break;
            }

            auto velo = VecToJPHVec(veloctiy);


            JPH::BodyCreationSettings bodyCreationSettings(shape->shapeRef, pos, quat, motionType, objectLayer);
            bodyCreationSettings.mLinearVelocity = JPH::Vec3(veloctiy.x, veloctiy.y, veloctiy.z);

            return bodyInterface.CreateAndAddBody(bodyCreationSettings, JPH::EActivation::Activate);

        }

        void PhysicsWorld::SetBodyMatrix(JPH::BodyID bodyId, const mat4& matrix) {

            JPH::Vec3 pos;
            JPH::Quat quat;
            MatrixToJPHPosAndRot(matrix, pos, quat);

            auto& bodyInterface = system.GetBodyInterface();
            bodyInterface.SetPositionAndRotation(bodyId, pos, quat, JPH::EActivation::Activate);

        }

        mat4 PhysicsWorld::GetBodyMatrix(JPH::BodyID bodyId) {

            auto& bodyInterface = system.GetBodyInterface();

            auto transform = bodyInterface.GetWorldTransform(bodyId);

            mat4 matrix;
            for (int8_t i = 0; i < 4; i++) {
                auto col = transform.GetColumn4(i);
                matrix[i] = vec4(col.GetX(), col.GetY(), col.GetZ(), col.GetW());
            }

            return matrix;

        }

        void PhysicsWorld::SetLinearVelocity(JPH::BodyID bodyId, glm::vec3 velocity) {

            auto& bodyInterface = system.GetBodyInterface();

            bodyInterface.SetLinearVelocity(bodyId, VecToJPHVec(velocity));

        }

        vec3 PhysicsWorld::GetLinearVelocity(JPH::BodyID bodyId) {

            auto& bodyInterface = system.GetBodyInterface();

            return JPHVecToVec(bodyInterface.GetLinearVelocity(bodyId));

        }

        void PhysicsWorld::MatrixToJPHPosAndRot(const mat4& matrix, JPH::Vec3& pos, JPH::Quat& quat) {

            Common::MatrixDecomposition decomp(matrix);

            pos = JPH::Vec3(decomp.translation.x, decomp.translation.y, decomp.translation.z);
            quat = JPH::Quat(decomp.quat.x, decomp.quat.y, decomp.quat.z, decomp.quat.w);

        }

        void PhysicsWorld::JPHPosAndRotToMatrix(const JPH::Vec3& pos, const JPH::Quat& quat, mat4& matrix) {



        }

        JPH::Vec3 PhysicsWorld::VecToJPHVec(const glm::vec3 vec) {

            return JPH::Vec3(vec.x, vec.y, vec.z);

        }

        vec3 PhysicsWorld::JPHVecToVec(const JPH::Vec3 vec) {

            return vec3(vec.GetX(), vec.GetY(), vec.GetZ());

        }

    }

}