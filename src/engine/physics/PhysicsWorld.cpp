#include "PhysicsWorld.h"
#include "PhysicsManager.h"
#include "MathConversion.h"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>

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

            system = CreateRef<JPH::PhysicsSystem>();

            system->Init(maxBodyCount, bodyMutexesCount, maxBodyPairCount, maxContactConstraintCount,
                *this->broadPhaseLayerInterface, *this->objectVsBroadPhaseLayerFilter, *this->objectLayerFilter);

            state = CreateRef<JPH::StateRecorderImpl>();

            //contactListener = CreateRef<MyContactListener>();
            //system.SetContactListener(contactListener.get());

        }

        PhysicsWorld::~PhysicsWorld() {

            for (auto [_, shape] : bodyToShapeMap)
                shape->settings.reset();

            bodyToShapeMap.clear();

        }

        void PhysicsWorld::Update(float deltaTime) {

            if (pauseSimulation) return;

            PhysicsManager::ExecuteUpdate(this, deltaTime);

        }

        Body PhysicsWorld::CreateBody(const BodyCreationSettings& bodyCreationSettings, const mat4& matrix) {

            auto& bodyInterface = system->GetBodyInterface();

            JPH::Vec3 pos;
            JPH::Quat quat;
            MatrixToJPHPosAndRot(matrix, pos, quat);

            auto settings = bodyCreationSettings.GetSettings();

            settings.mPosition = pos;
            settings.mRotation = quat;

            auto bodyId = bodyInterface.CreateAndAddBody(settings, JPH::EActivation::Activate);
            bodyToShapeMap[bodyId] = bodyCreationSettings.shape;

            return bodyId;

        }

        void PhysicsWorld::DestroyBody(Body bodyId) {

            auto& bodyInterface = system->GetBodyInterface();

            bodyToShapeMap.erase(bodyId);

            bodyInterface.RemoveBody(bodyId);
            bodyInterface.DestroyBody(bodyId);

        }

        void PhysicsWorld::SetBodyMatrix(Body bodyId, const mat4& matrix) {

            JPH::Vec3 pos;
            JPH::Quat quat;
            MatrixToJPHPosAndRot(matrix, pos, quat);

            auto& bodyInterface = system->GetBodyInterface();
            bodyInterface.SetPositionAndRotation(bodyId, pos, quat, JPH::EActivation::Activate);

        }

        mat4 PhysicsWorld::GetBodyMatrix(Body bodyId) {

            auto& bodyInterface = system->GetBodyInterface();

            auto transform = bodyInterface.GetWorldTransform(bodyId);

            auto matrix = JPHMatToMat(transform);

            auto shapeRef = bodyInterface.GetShape(bodyId);
            // Need to scale here since the tranform doesn't include scale
            if (shapeRef->GetSubType() == JPH::EShapeSubType::Scaled) {
                auto shape = static_cast<const JPH::ScaledShape*>(shapeRef.GetPtr());

                matrix = matrix * glm::scale(glm::mat4(1.0f), JPHVecToVec(shape->GetScale()));
            }

            return matrix;

        }

        void PhysicsWorld::SetMotionQuality(Body bodyId, MotionQuality quality) {

            auto& bodyInterface = system->GetBodyInterface();
            bodyInterface.SetMotionQuality(bodyId, quality);

        }

        MotionQuality PhysicsWorld::GetMotionQuality(Body bodyId) {

            auto& bodyInterface = system->GetBodyInterface();
            return bodyInterface.GetMotionQuality(bodyId);

        }

        void PhysicsWorld::SetLinearVelocity(Body bodyId, glm::vec3 velocity) {

            auto& bodyInterface = system->GetBodyInterface();
            bodyInterface.SetLinearVelocity(bodyId, VecToJPHVec(velocity));

        }

        vec3 PhysicsWorld::GetLinearVelocity(Body bodyId) {

            auto& bodyInterface = system->GetBodyInterface();
            return JPHVecToVec(bodyInterface.GetLinearVelocity(bodyId));

        }

        void PhysicsWorld::SetRestitution(Body bodyId, float restitution) {

            auto& bodyInterface = system->GetBodyInterface();
            bodyInterface.SetRestitution(bodyId, restitution);

        }

        float PhysicsWorld::GetRestitution(Body bodyId) {

            auto& bodyInterface = system->GetBodyInterface();
            return bodyInterface.GetRestitution(bodyId);

        }

        void PhysicsWorld::SetFriction(Body bodyId, float friction) {

            auto& bodyInterface = system->GetBodyInterface();
            bodyInterface.SetFriction(bodyId, friction);

        }

        float PhysicsWorld::GetFriction(Body bodyId) {

            auto& bodyInterface = system->GetBodyInterface();
            return bodyInterface.GetFriction(bodyId);

        }

        void PhysicsWorld::ChangeShape(Body bodyId, Ref<Shape> shape) {

            auto& bodyInterface = system->GetBodyInterface();
            bodyInterface.SetShape(bodyId, shape->ref, true, JPH::EActivation::Activate);

            bodyToShapeMap[bodyId] = shape;

        }

        BodyCreationSettings PhysicsWorld::GetBodyCreationSettings(Body bodyId) {

            auto& bodyLockInterface = system->GetBodyLockInterface();

            JPH::BodyLockRead lock(bodyLockInterface, bodyId);
            if (lock.Succeeded()) {
                const auto& body = lock.GetBody();

                if (body.IsRigidBody()) {
                    JPH::BodyCreationSettings settings = body.GetBodyCreationSettings();
                    BodyCreationSettings bodyCreationSettings;
                    bodyCreationSettings.SetSettings(settings);
                    bodyCreationSettings.shape = bodyToShapeMap[bodyId];
                    return bodyCreationSettings;
                }
            }

            return {};

        }

        void PhysicsWorld::SetGravity(vec3 gravity) {

            system->SetGravity(VecToJPHVec(gravity));

        }

        vec3 PhysicsWorld::GetGravity() {

            return JPHVecToVec(system->GetGravity());

        }

        void PhysicsWorld::OptimizeBroadphase() {

            system->OptimizeBroadPhase();

        }

        void PhysicsWorld::SaveState() {

            system->SaveState(*state);

        }

        void PhysicsWorld::RestoreState() {

            system->RestoreState(*state);
            state->Clear();

        }

    }

}