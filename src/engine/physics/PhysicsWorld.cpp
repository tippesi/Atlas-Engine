#include "PhysicsWorld.h"
#include "PhysicsManager.h"
#include "MathConversion.h"

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
                this->objectLayerFilter = CreateRef<ObjectLayerPairFilterImpl>();
            if (broadPhaseLayerInterface == nullptr)
                this->broadPhaseLayerInterface = CreateRef<BroadPhaseLayerInterfaceImpl>();
            if (objectVsBroadPhaseLayerFilter == nullptr)
                this->objectVsBroadPhaseLayerFilter = CreateRef<ObjectVsBroadPhaseLayerFilterImpl>();

            system.Init(maxBodyCount, bodyMutexesCount, maxBodyPairCount, maxContactConstraintCount,
                *this->broadPhaseLayerInterface, *this->objectVsBroadPhaseLayerFilter, *this->objectLayerFilter);

            //contactListener = CreateRef<MyContactListener>();
            //system.SetContactListener(contactListener.get());

        }

        void PhysicsWorld::Update(float deltaTime) {

            PhysicsManager::ExecuteUpdate(this, deltaTime);

        }

        Body PhysicsWorld::CreateBody(const Ref<ShapesManager> &shape, JPH::ObjectLayer objectLayer,
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

            JPH::BodyCreationSettings bodyCreationSettings(shape->shapeRef, pos, quat, motionType, objectLayer);
            bodyCreationSettings.mLinearVelocity = VecToJPHVec(veloctiy);
            bodyCreationSettings.mFriction = 1.0f;
            bodyCreationSettings.mRestitution = 0.2f;

            return bodyInterface.CreateAndAddBody(bodyCreationSettings, JPH::EActivation::Activate);

        }

        void PhysicsWorld::DestroyBody(Body bodyId) {

            auto& bodyInterface = system.GetBodyInterface();

            bodyInterface.RemoveBody(bodyId);
            bodyInterface.DestroyBody(bodyId);

        }

        void PhysicsWorld::SetBodyMatrix(Body bodyId, const mat4& matrix) {

            JPH::Vec3 pos;
            JPH::Quat quat;
            MatrixToJPHPosAndRot(matrix, pos, quat);

            auto& bodyInterface = system.GetBodyInterface();
            bodyInterface.SetPositionAndRotation(bodyId, pos, quat, JPH::EActivation::Activate);

        }

        mat4 PhysicsWorld::GetBodyMatrix(Body bodyId) {

            auto& bodyInterface = system.GetBodyInterface();

            auto transform = bodyInterface.GetWorldTransform(bodyId);

            mat4 matrix;
            for (int8_t i = 0; i < 4; i++) {
                auto col = transform.GetColumn4(i);
                matrix[i] = vec4(col.GetX(), col.GetY(), col.GetZ(), col.GetW());
            }

            return matrix;

        }

        void PhysicsWorld::SetLinearVelocity(Body bodyId, glm::vec3 velocity) {

            auto& bodyInterface = system.GetBodyInterface();

            bodyInterface.SetLinearVelocity(bodyId, VecToJPHVec(velocity));

        }

        vec3 PhysicsWorld::GetLinearVelocity(Body bodyId) {

            auto& bodyInterface = system.GetBodyInterface();

            return JPHVecToVec(bodyInterface.GetLinearVelocity(bodyId));

        }

        void PhysicsWorld::OptimizeBroadphase() {

            system.OptimizeBroadPhase();

        }

    }

}