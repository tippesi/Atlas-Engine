#include "PhysicsWorld.h"
#include "PhysicsManager.h"
#include "../profiler/CPUProfiler.h"
#include "MathConversion.h"
#include "../renderer/PrimitiveBatch.h"
#include "../tools/PerformanceCounter.h"
#include "../common/MathHelper.h"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseQuery.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/ConeConstraint.h>

namespace Atlas {

    namespace Physics {

        // Ignored bodies are filtered in the queries. Continuing a query behind them would start inside of convex
        // shapes, which count as solid and are hit again right away
        class QueryBodyFilter : public JPH::BodyFilter {
        public:
            QueryBodyFilter(PhysicsWorld* world, const std::function<bool(const Body&)>& ignore) : world(world), ignore(ignore) {}

            bool ShouldCollide(const JPH::BodyID& bodyId) const override { return !ignore || !ignore(Body(bodyId, world)); }

            bool ShouldCollideLocked(const JPH::Body& body) const override { return !body.IsSensor(); }

        private:
            PhysicsWorld* world;
            const std::function<bool(const Body&)>& ignore;
        };

        class BuoyancyStepListener : public JPH::PhysicsStepListener {
        public:
            explicit BuoyancyStepListener(PhysicsWorld* world) : world(world) {}

            void OnStep(const JPH::PhysicsStepListenerContext& inContext) override { world->ApplyBuoyancy(inContext.mDeltaTime); }

        private:
            PhysicsWorld* world;
        };

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

            contactListener = CreateRef<ContactListener>();
            system->SetContactListener(contactListener.get());

            stepListener = CreateRef<BuoyancyStepListener>(this);
            system->AddStepListener(stepListener.get());

        }

        PhysicsWorld::~PhysicsWorld() {

            system->RemoveStepListener(stepListener.get());

            for (auto& constraint : constraints)
                system->RemoveConstraint(constraint);
            constraints.clear();

            for (auto [_, shape] : bodyToShapeMap)
                shape->settings.reset();

            bodyToShapeMap.clear();

        }

        void PhysicsWorld::Update(float deltaTime) {

            if (pauseSimulation) return;

            CPUProfiler::Scope profilerScope("Physics simulation");

            // A long frame (the first one after loading, a hitch) doesn't take giant steps, which throw floating
            // bodies out of the water: the simulation falls behind instead
            const int32_t maxCollisionSteps = 8;
            deltaTime = std::min(deltaTime, float(maxCollisionSteps) / float(simulationStepsPerSecond));
            auto collisionSteps = std::min(std::max(int32_t(float(simulationStepsPerSecond) * deltaTime), 1), maxCollisionSteps);
            stepDeltaTime = deltaTime / float(collisionSteps);

            FindFluidContacts();

            // Waves keep moving, bodies on them shouldn't fall asleep
            auto& bodyInterface = system->GetBodyInterface();
            for (const auto& [bodyId, _] : bodyFluids)
                if (bodyToBuoyancyMap.contains(bodyId))
                    bodyInterface.ActivateBody(bodyId);

            Tools::PerformanceCounter performanceCounter;

            PhysicsManager::ExecuteUpdate(this, deltaTime, collisionSteps);

            updateTime = float(performanceCounter.Stamp().delta);

            ProcessContacts();

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

            auto activation = bodyCreationSettings.startAsleep ? JPH::EActivation::DontActivate : JPH::EActivation::Activate;
            auto bodyId = bodyInterface.CreateAndAddBody(settings, activation);
            bodyToShapeMap[bodyId] = bodyCreationSettings.shape;
            RecordStaticChange(bodyId);

            if (bodyCreationSettings.buoyancy > 0.0f && settings.mMotionType == JPH::EMotionType::Dynamic)
                bodyToBuoyancyMap[bodyId] = { bodyCreationSettings.buoyancy,
                    bodyCreationSettings.fluidLinearDrag, bodyCreationSettings.fluidAngularDrag };
            if (bodyCreationSettings.isFluid && settings.mIsSensor)
                fluidBodies.insert(bodyId);
            if (bodyCreationSettings.startAsleep)
                startAsleepBodies.insert(bodyId);

            return { bodyId, this };

        }

        void PhysicsWorld::DestroyBody(Body body) {

            auto& bodyInterface = system->GetBodyInterface();

            // Constraints keep pointers to their bodies
            std::erase_if(constraints, [&](const JPH::Ref<JPH::TwoBodyConstraint>& constraint) {
                if (constraint->GetBody1()->GetID() != body.bodyId && constraint->GetBody2()->GetID() != body.bodyId)
                    return false;
                bodyInterface.ActivateConstraint(constraint);
                system->RemoveConstraint(constraint);
                return true;
                });

            std::erase_if(contactPairs, [&](const auto& item) {
                const auto& event = item.second.event;
                if (event.bodyId != body.bodyId && event.otherBodyId != body.bodyId)
                    return false;
                EndContact(item.second);
                return true;
                });

            RecordStaticChange(body.bodyId);
            bodyToShapeMap.erase(body.bodyId);
            bodyToBuoyancyMap.erase(body.bodyId);
            bodyToFluidBodiesMap.erase(body.bodyId);
            bodyFluids.erase(body.bodyId);
            fluidBodies.erase(body.bodyId);
            startAsleepBodies.erase(body.bodyId);

            bodyInterface.RemoveBody(body.bodyId);
            bodyInterface.DestroyBody(body.bodyId);

        }

        bool PhysicsWorld::ContainsBody(Body body) const {

            if (body.bodyId.IsInvalid())
                return false;

            return bodyToShapeMap.contains(body.bodyId);

        }

        void PhysicsWorld::SetBodyMatrix(BodyID bodyId, const mat4& matrix) {

            JPH::Vec3 pos;
            JPH::Quat quat;
            MatrixToJPHPosAndRot(matrix, pos, quat);

            auto& bodyInterface = system->GetBodyInterface();
            // A sleeping body set where it already is stays asleep, e.g. every body in the first update after loading
            auto activation = JPH::EActivation::Activate;
            if (!bodyInterface.IsActive(bodyId) && bodyInterface.GetPosition(bodyId).IsClose(pos, 1e-6f) &&
                bodyInterface.GetRotation(bodyId).IsClose(quat, 1e-6f))
                activation = JPH::EActivation::DontActivate;
            RecordStaticChange(bodyId);
            bodyInterface.SetPositionAndRotation(bodyId, pos, quat, activation);
            RecordStaticChange(bodyId);

        }

        void PhysicsWorld::MoveKinematicBody(BodyID bodyId, const mat4& matrix, float deltaTime) {

            JPH::Vec3 pos;
            JPH::Quat quat;
            MatrixToJPHPosAndRot(matrix, pos, quat);

            auto& bodyInterface = system->GetBodyInterface();
            bodyInterface.MoveKinematic(bodyId, pos, quat, deltaTime);

        }

        void PhysicsWorld::StopKinematicBody(BodyID bodyId) {

            auto& bodyInterface = system->GetBodyInterface();
            bodyInterface.SetLinearAndAngularVelocity(bodyId, JPH::Vec3::sZero(), JPH::Vec3::sZero());

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

        bool PhysicsWorld::IsActive(BodyID bodyId) {

            auto& bodyInterface = system->GetBodyInterface();
            return bodyInterface.IsActive(bodyId);

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

        void PhysicsWorld::AddForce(BodyID bodyId, vec3 force) {

            auto& bodyInterface = system->GetBodyInterface();
            bodyInterface.AddForce(bodyId, VecToJPHVec(force));

        }

        void PhysicsWorld::AddForce(BodyID bodyId, vec3 force, vec3 position) {

            auto& bodyInterface = system->GetBodyInterface();
            bodyInterface.AddForce(bodyId, VecToJPHVec(force), VecToJPHVec(position));

        }

        void PhysicsWorld::AddImpulse(BodyID bodyId, vec3 impulse) {

            auto& bodyInterface = system->GetBodyInterface();
            bodyInterface.AddImpulse(bodyId, VecToJPHVec(impulse));

        }

        void PhysicsWorld::AddImpulse(BodyID bodyId, vec3 impulse, vec3 position) {

            auto& bodyInterface = system->GetBodyInterface();
            bodyInterface.AddImpulse(bodyId, VecToJPHVec(impulse), VecToJPHVec(position));

        }

        void PhysicsWorld::AddTorque(BodyID bodyId, vec3 torque) {

            auto& bodyInterface = system->GetBodyInterface();
            bodyInterface.AddTorque(bodyId, VecToJPHVec(torque));

        }

        void PhysicsWorld::AddAngularImpulse(BodyID bodyId, vec3 angularImpulse) {

            auto& bodyInterface = system->GetBodyInterface();
            bodyInterface.AddAngularImpulse(bodyId, VecToJPHVec(angularImpulse));

        }

        uint64_t PhysicsWorld::GetUserData(BodyID bodyId) {

            auto& bodyInterface = system->GetBodyInterface();
            return bodyInterface.GetUserData(bodyId);

        }

        void PhysicsWorld::ChangeShape(BodyID bodyId, Ref<Shape> shape) {

            auto& bodyInterface = system->GetBodyInterface();
            RecordStaticChange(bodyId);
            bodyInterface.SetShape(bodyId, shape->ref, true, JPH::EActivation::Activate);
            RecordStaticChange(bodyId);

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
                    bodyCreationSettings.isFluid = fluidBodies.contains(bodyId);

                    auto it = bodyToBuoyancyMap.find(bodyId);
                    if (it != bodyToBuoyancyMap.end()) {
                        bodyCreationSettings.buoyancy = it->second.buoyancy;
                        bodyCreationSettings.fluidLinearDrag = it->second.linearDrag;
                        bodyCreationSettings.fluidAngularDrag = it->second.angularDrag;
                    }
                    bodyCreationSettings.startAsleep = startAsleepBodies.contains(bodyId);
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

        bool PhysicsWorld::HasBuoyancy(BodyID bodyId) const {

            return bodyToBuoyancyMap.contains(bodyId);

        }

        Volume::AABB PhysicsWorld::GetBodyBounds(BodyID bodyId) const {

            auto& bodyInterface = system->GetBodyInterface();
            auto bounds = bodyInterface.GetTransformedShape(bodyId).GetWorldSpaceBounds();

            return Volume::AABB(JPHVecToVec(bounds.mMin), JPHVecToVec(bounds.mMax));

        }

        uint32_t PhysicsWorld::GetBodyCount() const {

            return system->GetNumBodies();

        }

        WorldStatistics PhysicsWorld::GetStatistics() const {

            auto bodyStats = system->GetBodyStats();

            WorldStatistics statistics;
            statistics.bodyCount = bodyStats.mNumBodies;
            statistics.staticBodyCount = bodyStats.mNumBodiesStatic;
            statistics.dynamicBodyCount = bodyStats.mNumBodiesDynamic;
            statistics.activeDynamicBodyCount = bodyStats.mNumActiveBodiesDynamic;
            statistics.kinematicBodyCount = bodyStats.mNumBodiesKinematic;
            statistics.activeKinematicBodyCount = bodyStats.mNumActiveBodiesKinematic;
            statistics.constraintCount = uint32_t(constraints.size());
            statistics.contactCount = uint32_t(contactPairs.size());
            statistics.updateTime = updateTime;

            return statistics;

        }

        std::vector<Body> PhysicsWorld::QueryAABB(const Volume::AABB& aabb) {

            JPH::AllHitCollisionCollector<JPH::CollideShapeBodyCollector> collector;
            system->GetBroadPhaseQuery().CollideAABox(JPH::AABox(VecToJPHVec(aabb.min), VecToJPHVec(aabb.max)), collector);

            std::vector<Body> bodies;
            for (auto bodyId : collector.mHits)
                bodies.emplace_back(bodyId, this);
            return bodies;

        }

        Volume::RayResult<Body> PhysicsWorld::CastRay(Volume::Ray& ray, const std::function<bool(const Body&)>& ignore) {

            JPH::RayCastResult hit;
            // The ray starts at tMin and the direction is the whole segment up to tMax
            auto length = ray.tMax - ray.tMin;
            JPH::RRayCast rayCast{ VecToJPHVec(ray.origin + ray.direction * ray.tMin),
                VecToJPHVec(ray.direction * length) };

            Volume::RayResult<Body> result;

            JPH::RayCastSettings rayCastSettings = {};

            rayCastSettings.SetBackFaceMode(JPH::EBackFaceMode::CollideWithBackFaces);

            QueryBodyFilter bodyFilter(this, ignore);
            if (system->GetNarrowPhaseQuery().CastRay(rayCast, hit, JPH::BroadPhaseLayerFilter(), JPH::ObjectLayerFilter(), bodyFilter)) {
                auto& bodyLockInterface = system->GetBodyLockInterface();

                JPH::BodyLockRead lock(bodyLockInterface, hit.mBodyID);
                if (lock.Succeeded()) {
                    const auto& body = lock.GetBody();

                    auto normal = body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, rayCast.GetPointOnRay(hit.mFraction));
                    result.normal = JPHVecToVec(normal);
                }

                result.valid = true;
                // Distances are measured from the ray origin like for all other ray queries
                result.hitDistance = ray.tMin + length * hit.mFraction;
                result.data = { hit.mBodyID, this };
            }

            return result;

        }

        Volume::RayResult<Body> PhysicsWorld::CastShape(const Ref<Shape>& shape, const mat4& matrix, vec3 direction,
            float distance, const std::function<bool(const Body&)>& ignore) {

            Volume::RayResult<Body> result;

            if (!shape->IsValid() || distance <= 0.0f)
                return result;

            JPH::Vec3 pos;
            JPH::Quat quat;
            MatrixToJPHPosAndRot(matrix, pos, quat);

            auto shapeCast = JPH::RShapeCast::sFromWorldTransform(shape->ref, JPH::Vec3::sOne(),
                JPH::Mat44::sRotationTranslation(quat, pos), VecToJPHVec(glm::normalize(direction) * distance));

            JPH::ShapeCastSettings shapeCastSettings;
            shapeCastSettings.mBackFaceModeTriangles = JPH::EBackFaceMode::CollideWithBackFaces;

            JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
            QueryBodyFilter bodyFilter(this, ignore);
            system->GetNarrowPhaseQuery().CastShape(shapeCast, shapeCastSettings, JPH::RVec3::sZero(), collector,
                JPH::BroadPhaseLayerFilter(), JPH::ObjectLayerFilter(), bodyFilter);

            if (collector.HadHit()) {
                const auto& hit = collector.mHit;

                result.valid = true;
                result.hitDistance = distance * hit.mFraction;
                result.normal = -JPHVecToVec(hit.mPenetrationAxis.NormalizedOr(JPH::Vec3::sZero()));
                result.data = { hit.mBodyID2, this };
            }

            return result;

        }

        std::vector<Body> PhysicsWorld::CollideShape(const Ref<Shape>& shape, const mat4& matrix,
            const std::function<bool(const Body&)>& ignore) {

            std::vector<Body> bodies;

            if (!shape->IsValid())
                return bodies;

            JPH::Vec3 pos;
            JPH::Quat quat;
            MatrixToJPHPosAndRot(matrix, pos, quat);

            auto centerOfMassTransform = JPH::Mat44::sRotationTranslation(quat, pos).PreTranslated(shape->ref->GetCenterOfMass());

            JPH::CollideShapeSettings collideShapeSettings;
            collideShapeSettings.mBackFaceMode = JPH::EBackFaceMode::CollideWithBackFaces;

            JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
            QueryBodyFilter bodyFilter(this, ignore);
            system->GetNarrowPhaseQuery().CollideShape(shape->ref, JPH::Vec3::sOne(), centerOfMassTransform,
                collideShapeSettings, JPH::RVec3::sZero(), collector, JPH::BroadPhaseLayerFilter(),
                JPH::ObjectLayerFilter(), bodyFilter);

            for (const auto& hit : collector.mHits) {
                auto found = std::find_if(bodies.begin(), bodies.end(),
                    [&](const Body& body) { return body.bodyId == hit.mBodyID2; });
                if (found == bodies.end())
                    bodies.emplace_back(hit.mBodyID2, this);
            }

            return bodies;

        }

        Constraint PhysicsWorld::CreateConstraint(const ConstraintCreationSettings& constraintCreationSettings,
            BodyID bodyId, BodyID otherBodyId) {

            const auto& settings = constraintCreationSettings;

            auto matrix = GetBodyMatrix(bodyId);
            auto point = VecToJPHVec(vec3(matrix * vec4(settings.point, 1.0f)));
            auto worldAxis = mat3(matrix) * settings.axis;
            auto axis = VecToJPHVec(glm::length(worldAxis) > 0.0f ? glm::normalize(worldAxis) : vec3(0.0f, 1.0f, 0.0f));
            auto normal = axis.GetNormalizedPerpendicular();

            auto otherPoint = VecToJPHVec(settings.otherPoint);
            if (!otherBodyId.IsInvalid())
                otherPoint = VecToJPHVec(vec3(GetBodyMatrix(otherBodyId) * vec4(settings.otherPoint, 1.0f)));

            JPH::Ref<JPH::TwoBodyConstraintSettings> constraintSettings;
            switch (settings.type) {
                case ConstraintType::Fixed: {
                    auto fixedSettings = new JPH::FixedConstraintSettings();
                    fixedSettings->mAutoDetectPoint = true;
                    constraintSettings = fixedSettings;
                    break;
                }
                case ConstraintType::Point: {
                    auto pointSettings = new JPH::PointConstraintSettings();
                    pointSettings->mPoint1 = pointSettings->mPoint2 = point;
                    constraintSettings = pointSettings;
                    break;
                }
                case ConstraintType::Hinge: {
                    auto hingeSettings = new JPH::HingeConstraintSettings();
                    hingeSettings->mPoint1 = hingeSettings->mPoint2 = point;
                    hingeSettings->mHingeAxis1 = hingeSettings->mHingeAxis2 = axis;
                    hingeSettings->mNormalAxis1 = hingeSettings->mNormalAxis2 = normal;
                    if (settings.enableLimits) {
                        hingeSettings->mLimitsMin = glm::clamp(settings.limitsMin, -glm::pi<float>(), 0.0f);
                        hingeSettings->mLimitsMax = glm::clamp(settings.limitsMax, 0.0f, glm::pi<float>());
                    }
                    constraintSettings = hingeSettings;
                    break;
                }
                case ConstraintType::Slider: {
                    auto sliderSettings = new JPH::SliderConstraintSettings();
                    sliderSettings->mAutoDetectPoint = true;
                    sliderSettings->SetSliderAxis(axis);
                    if (settings.enableLimits) {
                        sliderSettings->mLimitsMin = glm::min(settings.limitsMin, 0.0f);
                        sliderSettings->mLimitsMax = glm::max(settings.limitsMax, 0.0f);
                    }
                    constraintSettings = sliderSettings;
                    break;
                }
                case ConstraintType::Distance: {
                    auto distanceSettings = new JPH::DistanceConstraintSettings();
                    distanceSettings->mPoint1 = point;
                    distanceSettings->mPoint2 = otherPoint;
                    // Without a range the distance at creation is kept
                    if (settings.enableLimits && settings.limitsMax > 0.0f) {
                        distanceSettings->mMinDistance = glm::max(settings.limitsMin, 0.0f);
                        distanceSettings->mMaxDistance = glm::max(settings.limitsMax, distanceSettings->mMinDistance);
                    }
                    constraintSettings = distanceSettings;
                    break;
                }
                case ConstraintType::Cone: {
                    auto coneSettings = new JPH::ConeConstraintSettings();
                    coneSettings->mPoint1 = coneSettings->mPoint2 = point;
                    coneSettings->mTwistAxis1 = coneSettings->mTwistAxis2 = axis;
                    coneSettings->mHalfConeAngle = glm::clamp(settings.halfConeAngle, 0.0f, glm::pi<float>());
                    constraintSettings = coneSettings;
                    break;
                }
            }

            auto& bodyInterface = system->GetBodyInterface();

            JPH::Ref<JPH::TwoBodyConstraint> constraint = bodyInterface.CreateConstraint(constraintSettings, bodyId, otherBodyId);
            system->AddConstraint(constraint);
            // Bodies asleep since loading stay asleep, e.g. a moored boat or a hanging cage, an awake one wakes the other
            auto otherActive = !otherBodyId.IsInvalid() && bodyInterface.IsActive(otherBodyId);
            if (bodyInterface.IsActive(bodyId) || otherActive)
                bodyInterface.ActivateConstraint(constraint);

            constraints.push_back(constraint);

            return { constraint, this };

        }

        void PhysicsWorld::DestroyConstraint(Constraint constraint) {

            if (!ContainsConstraint(constraint))
                return;

            system->GetBodyInterface().ActivateConstraint(constraint.ref);
            system->RemoveConstraint(constraint.ref);

            std::erase(constraints, constraint.ref);

        }

        bool PhysicsWorld::ContainsConstraint(const Constraint& constraint) const {

            if (constraint.ref == nullptr)
                return false;

            return std::find(constraints.begin(), constraints.end(), constraint.ref) != constraints.end();

        }

        void PhysicsWorld::ProcessContacts() {

            auto& bodyInterface = system->GetBodyInterface();

            auto pairKey = [](BodyID bodyId, BodyID otherBodyId) {
                return (uint64_t(bodyId.GetIndexAndSequenceNumber()) << 32) | otherBodyId.GetIndexAndSequenceNumber();
            };

            // Sleeping bodies lose their contacts, these only end when the bodies move apart after waking up
            auto isSleeping = [&](BodyID bodyId) {
                return bodyInterface.IsAdded(bodyId) && !bodyInterface.IsActive(bodyId) &&
                    bodyInterface.GetMotionType(bodyId) == JPH::EMotionType::Dynamic;
            };

            // Contacts are reported per sub shape, a body pair begins with its first and ends with its last contact
            for (const auto& event : contactListener->addedContacts) {
                auto& contactPair = contactPairs[pairKey(event.bodyId, event.otherBodyId)];
                if (contactPair.count++ == 0 && !contactPair.sleeping) {
                    contactPair.event = event;
                    contactEvents.push_back(event);
                }
                contactPair.sleeping = false;
            }

            for (const auto& [bodyId, otherBodyId] : contactListener->removedContacts) {
                auto it = contactPairs.find(pairKey(bodyId, otherBodyId));
                if (it == contactPairs.end() || it->second.count == 0 || --it->second.count > 0)
                    continue;

                if (!it->second.event.isTrigger && (isSleeping(bodyId) || isSleeping(otherBodyId))) {
                    it->second.sleeping = true;
                    continue;
                }

                EndContact(it->second);
                contactPairs.erase(it);
            }

            std::erase_if(contactPairs, [&](const auto& item) {
                const auto& contactPair = item.second;
                if (!contactPair.sleeping || isSleeping(contactPair.event.bodyId) ||
                    isSleeping(contactPair.event.otherBodyId))
                    return false;
                EndContact(contactPair);
                return true;
                });

            for (const auto& event : contactListener->persistedContacts) {
                auto it = contactPairs.find(pairKey(event.bodyId, event.otherBodyId));
                if (it == contactPairs.end())
                    continue;

                it->second.event.position = event.position;
                it->second.event.normal = event.normal;
            }

            contactListener->addedContacts.clear();
            contactListener->persistedContacts.clear();
            contactListener->removedContacts.clear();

            // Debug drawing requests the persisted contacts again for the next update
            contactListener->recordPersistedContacts = false;

        }

        void PhysicsWorld::FindFluidContacts() {

            bodyToFluidBodiesMap.clear();

            if (bodyToBuoyancyMap.empty() || fluidBodies.empty())
                return;

            for (const auto& [_, contactPair] : contactPairs) {
                const auto& event = contactPair.event;
                if (fluidBodies.contains(event.bodyId) && bodyToBuoyancyMap.contains(event.otherBodyId))
                    bodyToFluidBodiesMap[event.otherBodyId].push_back(event.bodyId);
                if (fluidBodies.contains(event.otherBodyId) && bodyToBuoyancyMap.contains(event.bodyId))
                    bodyToFluidBodiesMap[event.bodyId].push_back(event.otherBodyId);
            }

        }

        void PhysicsWorld::ApplyBuoyancy(float deltaTime) {

            // All bodies are locked during the collision steps
            const auto& bodyLockInterface = system->GetBodyLockInterfaceNoLock();
            auto gravity = system->GetGravity();

            for (const auto& [bodyId, buoyancy] : bodyToBuoyancyMap) {
                JPH::BodyLockWrite lock(bodyLockInterface, bodyId);
                if (!lock.Succeeded())
                    continue;

                auto& body = lock.GetBody();
                if (!body.IsActive())
                    continue;

                auto position = JPHVecToVec(body.GetCenterOfMassPosition());

                // Bodies float in the fluid in which they are submerged the most
                Fluid fluid;
                auto depth = 0.0f;
                auto found = false;
                auto findDeepestFluid = [&](const Fluid& candidate) {
                    auto candidateDepth = glm::dot(candidate.surfacePosition - position, candidate.surfaceNormal);
                    if (found && candidateDepth <= depth)
                        return;
                    fluid = candidate;
                    depth = candidateDepth;
                    found = true;
                };

                // A fluid of the body itself, e.g. the waves below it, replaces the endless fluids
                auto bodyFluid = bodyFluids.find(bodyId);
                if (bodyFluid != bodyFluids.end()) {
                    findDeepestFluid(bodyFluid->second);
                }
                else {
                    for (const auto& candidate : fluids)
                        findDeepestFluid(candidate);
                }

                auto it = bodyToFluidBodiesMap.find(bodyId);
                if (it != bodyToFluidBodiesMap.end()) {
                    for (auto fluidBodyId : it->second) {
                        JPH::BodyLockRead fluidLock(bodyLockInterface, fluidBodyId);
                        if (!fluidLock.Succeeded())
                            continue;

                        const auto& fluidBody = fluidLock.GetBody();
                        auto surfaceHeight = fluidBody.GetWorldSpaceBounds().mMax.GetY();
                        findDeepestFluid({ .surfacePosition = vec3(position.x, surfaceHeight, position.z),
                            .velocity = JPHVecToVec(fluidBody.GetLinearVelocity()) });
                    }
                }

                if (!found)
                    continue;

                body.ApplyBuoyancyImpulse(VecToJPHVec(fluid.surfacePosition), VecToJPHVec(fluid.surfaceNormal),
                    buoyancy.buoyancy, buoyancy.linearDrag, buoyancy.angularDrag, VecToJPHVec(fluid.velocity),
                    gravity, deltaTime);
            }

        }

        const PhysicsWorld::DebugWireframe& PhysicsWorld::GetDebugWireframe(const JPH::Shape* shape) {

            auto& wireframe = debugWireframes[shape];
            wireframe.lastFrame = debugFrame;
            if (wireframe.shape)
                return wireframe;
            wireframe.shape = shape;

            struct Edge {
                vec3 v0, v1;
                vec3 normal;
                bool keep = true;
            };
            std::unordered_map<uint64_t, Edge> edges;
            // On a millimeter grid, so the two triangles of an edge find each other
            auto key = [](vec3 v) {
                auto q = glm::ivec3(glm::round(v * 1000.0f));
                return (uint64_t(uint32_t(q.x)) * 73856093u) ^ (uint64_t(uint32_t(q.y)) * 19349663u) ^ (uint64_t(uint32_t(q.z)) * 83492791u);
            };
            auto addEdge = [&](vec3 v0, vec3 v1, vec3 normal) {
                auto k0 = key(v0), k1 = key(v1);
                auto edgeKey = k0 < k1 ? k0 * 31u + k1 : k1 * 31u + k0;
                auto [it, inserted] = edges.try_emplace(edgeKey, Edge { v0, v1, normal });
                // An edge between two triangles of one plane lies inside a flat face
                if (!inserted)
                    it->second.keep = glm::dot(it->second.normal, normal) < 0.999f;
            };

            auto everything = JPH::AABox::sBiggest();
            JPH::TransformedShape transformedShape(JPH::RVec3::sZero(), JPH::Quat::sIdentity(), shape, JPH::BodyID());
            JPH::AllHitCollisionCollector<JPH::TransformedShapeCollector> collector;
            transformedShape.CollectTransformedShapes(everything, collector);

            JPH::Shape::GetTrianglesContext context;
            std::vector<JPH::Float3> vertices(3 * 256);
            for (const auto& leaf : collector.mHits) {
                leaf.GetTrianglesStart(context, everything, JPH::RVec3::sZero());
                int32_t count = 0;
                while ((count = leaf.GetTrianglesNext(context, 256, vertices.data())) > 0) {
                    for (int32_t i = 0; i < count; i++) {
                        auto v0 = JPHVecToVec(JPH::Vec3(vertices[3 * i]));
                        auto v1 = JPHVecToVec(JPH::Vec3(vertices[3 * i + 1]));
                        auto v2 = JPHVecToVec(JPH::Vec3(vertices[3 * i + 2]));
                        auto normal = Helper::Math::NormalizeOrZero(glm::cross(v1 - v0, v2 - v0));
                        addEdge(v0, v1, normal);
                        addEdge(v1, v2, normal);
                        addEdge(v2, v0, normal);
                    }
                }
            }

            for (const auto& [_, edge] : edges) {
                if (!edge.keep)
                    continue;
                wireframe.lines.push_back(edge.v0);
                wireframe.lines.push_back(edge.v1);
            }
            return wireframe;

        }

        void PhysicsWorld::DebugDraw(Renderer::PrimitiveBatch* primitiveBatch, const DebugDrawSettings& settings) {

            contactListener->recordPersistedContacts = settings.drawContacts;

            const auto& bodyLockInterface = system->GetBodyLockInterface();

            auto center = VecToJPHVec(settings.center);
            auto extent = JPH::Vec3::sReplicate(settings.distance);
            JPH::AABox drawBox(center - extent, center + extent);

            auto bodyColor = [&](const JPH::Body& body) {
                if (body.IsSensor())
                    return fluidBodies.contains(body.GetID()) ? vec3(0.2f, 0.4f, 1.0f) : vec3(0.0f, 1.0f, 1.0f);
                if (body.IsStatic())
                    return vec3(0.5f);
                if (body.IsKinematic())
                    return vec3(1.0f, 0.5f, 0.0f);
                return body.IsActive() ? vec3(0.0f, 1.0f, 0.0f) : vec3(0.0f, 0.4f, 0.0f);
            };

            if (settings.drawShapes || settings.drawBounds) {
                JPH::AllHitCollisionCollector<JPH::CollideShapeBodyCollector> bodyCollector;
                system->GetBroadPhaseQuery().CollideAABox(drawBox, bodyCollector);

                JPH::Shape::GetTrianglesContext context;
                std::vector<JPH::Float3> vertices(3 * 256);
                debugFrame++;

                for (auto bodyId : bodyCollector.mHits) {
                    JPH::BodyLockRead lock(bodyLockInterface, bodyId);
                    if (!lock.Succeeded())
                        continue;

                    const auto& body = lock.GetBody();
                    if (body.IsStatic() && !settings.drawStaticBodies)
                        continue;

                    auto bounds = body.GetWorldSpaceBounds();

                    auto color = bodyColor(body);
                    if (settings.drawBounds)
                        primitiveBatch->AddLineAABB(Volume::AABB(JPHVecToVec(bounds.mMin), JPHVecToVec(bounds.mMax)), color);

                    if (!settings.drawShapes)
                        continue;

                    // Bodies smaller than the box come from the cache, only large ones like the terrain are cut to the box
                    if ((bounds.GetExtent() - extent).ReduceMax() < 0.0f) {
                        const auto& wireframe = GetDebugWireframe(body.GetShape());
                        auto transform = body.GetCenterOfMassTransform();
                        for (size_t i = 0; i + 1 < wireframe.lines.size(); i += 2) {
                            auto v0 = JPHVecToVec(transform * VecToJPHVec(wireframe.lines[i]));
                            auto v1 = JPHVecToVec(transform * VecToJPHVec(wireframe.lines[i + 1]));
                            primitiveBatch->AddLine(v0, v1, color, color);
                        }
                        continue;
                    }

                    // Triangles are only available for leaf shapes, the engine shapes are at least scaled
                    JPH::AllHitCollisionCollector<JPH::TransformedShapeCollector> collector;
                    body.GetTransformedShape().CollectTransformedShapes(drawBox, collector);

                    for (const auto& transformedShape : collector.mHits) {
                        transformedShape.GetTrianglesStart(context, drawBox, JPH::RVec3::sZero());

                        int32_t count = 0;
                        while ((count = transformedShape.GetTrianglesNext(context, 256, vertices.data())) > 0) {
                            for (int32_t i = 0; i < count; i++) {
                                auto v0 = JPHVecToVec(JPH::Vec3(vertices[3 * i]));
                                auto v1 = JPHVecToVec(JPH::Vec3(vertices[3 * i + 1]));
                                auto v2 = JPHVecToVec(JPH::Vec3(vertices[3 * i + 2]));

                                primitiveBatch->AddLine(v0, v1, color, color);
                                primitiveBatch->AddLine(v1, v2, color, color);
                                primitiveBatch->AddLine(v2, v0, color, color);
                            }
                        }
                    }
                }
            }

            std::erase_if(debugWireframes, [&](const auto& item) { return debugFrame - item.second.lastFrame > 300; });

            if (settings.drawContacts) {
                for (const auto& [_, contactPair] : contactPairs) {
                    const auto& event = contactPair.event;
                    if (contactPair.sleeping || !drawBox.Contains(VecToJPHVec(event.position)))
                        continue;

                    auto color = event.isTrigger ? vec3(0.0f, 1.0f, 1.0f) : vec3(1.0f, 0.0f, 0.0f);
                    primitiveBatch->AddLineCross(event.position, 0.05f, color);
                    primitiveBatch->AddLine(event.position, event.position + event.normal * 0.25f, color, color);
                }
            }

            if (settings.drawConstraints) {
                for (const auto& constraint : constraints) {
                    auto matrix = constraint->GetBody1()->GetCenterOfMassTransform() * constraint->GetConstraintToBody1Matrix();
                    auto otherMatrix = constraint->GetBody2()->GetCenterOfMassTransform() * constraint->GetConstraintToBody2Matrix();

                    auto point = JPHVecToVec(matrix.GetTranslation());
                    auto otherPoint = JPHVecToVec(otherMatrix.GetTranslation());
                    if (!drawBox.Contains(VecToJPHVec(point)))
                        continue;

                    auto color = constraint->GetEnabled() ? vec3(1.0f, 0.0f, 1.0f) : vec3(0.5f, 0.0f, 0.5f);
                    primitiveBatch->AddLineCross(point, 0.1f, color);
                    primitiveBatch->AddLineCross(otherPoint, 0.1f, color);
                    primitiveBatch->AddLine(point, otherPoint, color, color);

                    // The first axis of the constraint space is the hinge, slider or twist axis
                    auto subType = constraint->GetSubType();
                    if (subType == JPH::EConstraintSubType::Hinge || subType == JPH::EConstraintSubType::Slider ||
                        subType == JPH::EConstraintSubType::Cone) {
                        auto axis = JPHVecToVec(matrix.GetAxisX()) * 0.5f;
                        primitiveBatch->AddLine(point - axis, point + axis, color, color);
                    }
                }
            }

        }

        void PhysicsWorld::EndContact(const ContactPair& contactPair) {

            ContactEvent event;
            event.type = ContactEventType::End;
            event.bodyId = contactPair.event.bodyId;
            event.otherBodyId = contactPair.event.otherBodyId;
            event.userData = contactPair.event.userData;
            event.otherUserData = contactPair.event.otherUserData;
            event.isTrigger = contactPair.event.isTrigger;

            contactEvents.push_back(event);

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

            contactPairs.clear();
            contactEvents.clear();
            bodyToFluidBodiesMap.clear();
            bodyFluids.clear();

        }

        void PhysicsWorld::RecordStaticChange(BodyID bodyId) {

            JPH::BodyLockRead lock(system->GetBodyLockInterface(), bodyId);
            if (!lock.Succeeded())
                return;

            const auto& body = lock.GetBody();
            if (!body.IsStatic() || body.IsSensor())
                return;

            auto bounds = body.GetWorldSpaceBounds();
            staticChanges.Add(Volume::AABB(JPHVecToVec(bounds.mMin), JPHVecToVec(bounds.mMax)));

        }

    }

}
