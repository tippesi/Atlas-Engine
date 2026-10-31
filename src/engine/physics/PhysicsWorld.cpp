#include "PhysicsWorld.h"
#include "PhysicsManager.h"
#include "MathConversion.h"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
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

        }

        PhysicsWorld::~PhysicsWorld() {

            for (auto& constraint : constraints)
                system->RemoveConstraint(constraint);
            constraints.clear();

            for (auto [_, shape] : bodyToShapeMap)
                shape->settings.reset();

            bodyToShapeMap.clear();

        }

        void PhysicsWorld::Update(float deltaTime) {

            if (pauseSimulation) return;

            auto collisionSteps = std::min(std::max(int32_t(float(simulationStepsPerSecond) * deltaTime), 1), 8);
            stepDeltaTime = deltaTime / float(collisionSteps);

            PhysicsManager::ExecuteUpdate(this, deltaTime, collisionSteps);

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

            auto bodyId = bodyInterface.CreateAndAddBody(settings, JPH::EActivation::Activate);
            bodyToShapeMap[bodyId] = bodyCreationSettings.shape;

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

            bodyToShapeMap.erase(body.bodyId);

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
            bodyInterface.SetPositionAndRotation(bodyId, pos, quat, JPH::EActivation::Activate);

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

        uint32_t PhysicsWorld::GetBodyCount() const {

            return system->GetNumBodies();

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

            contactListener->addedContacts.clear();
            contactListener->removedContacts.clear();

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

        }

    }

}
