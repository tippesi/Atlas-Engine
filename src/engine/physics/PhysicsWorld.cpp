#include "PhysicsWorld.h"
#include "PhysicsManager.h"
#include "MathConversion.h"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

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

        Body PhysicsWorld::CreateBody(const BodyCreationSettings& bodyCreationSettings, const mat4& matrix, uint64_t userData) {

            auto& bodyInterface = system->GetBodyInterface();

            JPH::Vec3 pos;
            JPH::Quat quat;
            MatrixToJPHPosAndRot(matrix, pos, quat);

            auto settings = bodyCreationSettings.GetSettings();

            AE_ASSERT(bodyCreationSettings.shape->type != ShapeType::Mesh || settings.mMotionType != JPH::EMotionType::Dynamic &&
                "Mesh shape bodies should not be set to a dynamic motion type");
            AE_ASSERT(bodyCreationSettings.shape->type != ShapeType::HeightField || settings.mMotionType != JPH::EMotionType::Dynamic &&
                "Height field shape bodies should not be set to a dynamic motion type");

            settings.mPosition = pos;
            settings.mRotation = quat;
            settings.mUserData = userData;

            auto bodyId = bodyInterface.CreateAndAddBody(settings, JPH::EActivation::Activate);
            bodyToShapeMap[bodyId] = bodyCreationSettings.shape;

            return { bodyId, this };

        }

        void PhysicsWorld::DestroyBody(Body body) {

            auto& bodyInterface = system->GetBodyInterface();

            bodyToShapeMap.erase(body.bodyId);

            bodyInterface.RemoveBody(body.bodyId);
            bodyInterface.DestroyBody(body.bodyId);

        }

        void PhysicsWorld::SetBodyMatrix(BodyID bodyId, const mat4& matrix) {

            JPH::Vec3 pos;
            JPH::Quat quat;
            MatrixToJPHPosAndRot(matrix, pos, quat);

            auto& bodyInterface = system->GetBodyInterface();
            bodyInterface.SetPositionAndRotation(bodyId, pos, quat, JPH::EActivation::Activate);

        }

        mat4 PhysicsWorld::GetBodyMatrix(BodyID bodyId) {

            auto& bodyInterface = system->GetBodyInterface();

            auto transform = bodyInterface.GetWorldTransform(bodyId);

            auto matrix = JPHMatToMat(transform);

            auto shapeRef = bodyInterface.GetShape(bodyId);
            // Need to scale here since the tranform doesn't include scale
            if (shapeRef->GetSubType() == JPH::EShapeSubType::Scaled) {
                auto shape = static_cast<const JPH::ScaledShape*>(shapeRef.GetPtr());

                matrix = glm::scale(glm::mat4(matrix), JPHVecToVec(shape->GetScale()));
            }

            return matrix;

        }

        void PhysicsWorld::SetMotionQuality(BodyID bodyId, MotionQuality quality) {

            auto& bodyInterface = system->GetBodyInterface();
            bodyInterface.SetMotionQuality(bodyId, quality);

        }

        MotionQuality PhysicsWorld::GetMotionQuality(BodyID bodyId) {

            auto& bodyInterface = system->GetBodyInterface();
            return bodyInterface.GetMotionQuality(bodyId);

        }

        void PhysicsWorld::SetLinearVelocity(BodyID bodyId, vec3 velocity) {

            auto& bodyInterface = system->GetBodyInterface();
            bodyInterface.SetLinearVelocity(bodyId, VecToJPHVec(velocity));

        }

        vec3 PhysicsWorld::GetLinearVelocity(BodyID bodyId) {

            auto& bodyInterface = system->GetBodyInterface();
            return JPHVecToVec(bodyInterface.GetLinearVelocity(bodyId));

        }

        void PhysicsWorld::SetAngularVelocity(BodyID bodyId, vec3 velocity) {

            auto& bodyInterface = system->GetBodyInterface();
            bodyInterface.SetAngularVelocity(bodyId, VecToJPHVec(velocity));

        }

        vec3 PhysicsWorld::GetAngularVelocity(BodyID bodyId) {

            auto& bodyInterface = system->GetBodyInterface();
            return JPHVecToVec(bodyInterface.GetAngularVelocity(bodyId));

        }

        void PhysicsWorld::SetRestitution(BodyID bodyId, float restitution) {

            auto& bodyInterface = system->GetBodyInterface();
            bodyInterface.SetRestitution(bodyId, restitution);

        }

        float PhysicsWorld::GetRestitution(BodyID bodyId) {

            auto& bodyInterface = system->GetBodyInterface();
            return bodyInterface.GetRestitution(bodyId);

        }

        void PhysicsWorld::SetFriction(BodyID bodyId, float friction) {

            auto& bodyInterface = system->GetBodyInterface();
            bodyInterface.SetFriction(bodyId, friction);

        }

        float PhysicsWorld::GetFriction(BodyID bodyId) {

            auto& bodyInterface = system->GetBodyInterface();
            return bodyInterface.GetFriction(bodyId);

        }

        uint64_t PhysicsWorld::GetUserData(BodyID bodyId) {

            auto& bodyInterface = system->GetBodyInterface();
            return bodyInterface.GetUserData(bodyId);

        }

        void PhysicsWorld::ChangeShape(BodyID bodyId, Ref<Shape> shape) {

            auto& bodyInterface = system->GetBodyInterface();
            bodyInterface.SetShape(bodyId, shape->ref, true, JPH::EActivation::Activate);

            bodyToShapeMap[bodyId] = shape;

        }

        BodyCreationSettings PhysicsWorld::GetBodyCreationSettings(BodyID bodyId) {

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

        Volume::RayResult<Body> PhysicsWorld::CastRay(Volume::Ray& ray) {

            JPH::RayCastResult hit;
            // Actually direction is not really a direction but one endpoint
            JPH::RRayCast rayCast{ VecToJPHVec(ray.origin + ray.direction * ray.tMin),
                VecToJPHVec(ray.direction * ray.tMax) };

            Volume::RayResult<Body> result;

            JPH::RayCastSettings rayCastSettings = {
                .mBackFaceMode = JPH::EBackFaceMode::CollideWithBackFaces
            };

            if (system->GetNarrowPhaseQuery().CastRay(rayCast, hit)) {
                auto& bodyLockInterface = system->GetBodyLockInterface();

                JPH::BodyLockRead lock(bodyLockInterface, hit.mBodyID);
                if (lock.Succeeded()) {
                    const auto& body = lock.GetBody();

                    auto normal = body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, rayCast.GetPointOnRay(hit.mFraction));
                    result.normal = JPHVecToVec(normal);
                }

                result.valid = true;
                result.hitDistance = (ray.tMax - ray.tMin) * hit.mFraction;
                result.data = { hit.mBodyID, this };
            }

            return result;

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