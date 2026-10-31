#pragma once

#include "../System.h"
#include "../volume/Ray.h"
#include "ShapesManager.h"
#include "BodyCreation.h"
#include "Body.h"
#include "Player.h"
#include "Constraint.h"
#include "ContactListener.h"

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

            bool ContainsBody(Body body) const;

            void SetBodyMatrix(BodyID bodyId, const mat4& matrix);

            void MoveKinematicBody(BodyID bodyId, const mat4& matrix, float deltaTime);

            void StopKinematicBody(BodyID bodyId);

            mat4 GetBodyMatrix(BodyID bodyId);

            void SetMotionQuality(BodyID bodyId, MotionQuality quality);

            MotionQuality GetMotionQuality(BodyID bodyId);

            void SetLinearVelocity(BodyID bodyId, vec3 velocity);

            vec3 GetLinearVelocity(BodyID bodyId);

            void SetAngularVelocity(BodyID bodyId, vec3 velocity);

            vec3 GetAngularVelocity(BodyID bodyId);

            void SetRestitution(BodyID bodyId, float restitution);

            float GetRestitution(BodyID bodyId);

            void SetFriction(BodyID bodyId, float friction);

            float GetFriction(BodyID bodyId);

            void AddForce(BodyID bodyId, vec3 force);

            void AddForce(BodyID bodyId, vec3 force, vec3 position);

            void AddImpulse(BodyID bodyId, vec3 impulse);

            void AddImpulse(BodyID bodyId, vec3 impulse, vec3 position);

            void AddTorque(BodyID bodyId, vec3 torque);

            void AddAngularImpulse(BodyID bodyId, vec3 angularImpulse);

            uint64_t GetUserData(BodyID bodyId);

            void ChangeShape(BodyID bodyId, Ref<Shape> shape);

            BodyCreationSettings GetBodyCreationSettings(BodyID bodyId);

            void SetGravity(vec3 gravity);

            vec3 GetGravity();

            uint32_t GetBodyCount() const;

            // One of the bodies has to be movable, an invalid other body connects the body to the world
            Constraint CreateConstraint(const ConstraintCreationSettings& constraintCreationSettings,
                BodyID bodyId, BodyID otherBodyId = BodyID());

            void DestroyConstraint(Constraint constraint);

            bool ContainsConstraint(const Constraint& constraint) const;

            // Queries pass through triggers and through bodies for which ignore returns true
            Volume::RayResult<Body> CastRay(Volume::Ray& ray, const std::function<bool(const Body&)>& ignore = {});

            // Moves the shape from the matrix along the direction, the hit distance is measured along the direction
            Volume::RayResult<Body> CastShape(const Ref<Shape>& shape, const mat4& matrix, vec3 direction,
                float distance, const std::function<bool(const Body&)>& ignore = {});

            // Returns all bodies that overlap the shape at the matrix
            std::vector<Body> CollideShape(const Ref<Shape>& shape, const mat4& matrix,
                const std::function<bool(const Body&)>& ignore = {});

            void OptimizeBroadphase();

            void SaveState();

            void RestoreState();

            Ref<JPH::PhysicsSystem> system = nullptr;

            int32_t simulationStepsPerSecond = 60;
            bool pauseSimulation = false;

            std::unordered_map<BodyID, Ref<Shape>> bodyToShapeMap;

            // Contacts that began and ended since the events were last cleared
            std::vector<ContactEvent> contactEvents;

        private:
            struct ContactPair {
                ContactEvent event;
                int32_t count = 0;
                bool sleeping = false;
            };

            void ProcessContacts();

            void EndContact(const ContactPair& contactPair);

            Ref<JPH::ObjectLayerPairFilter> objectLayerFilter = nullptr;
            Ref<JPH::BroadPhaseLayerInterface> broadPhaseLayerInterface = nullptr;
            Ref<JPH::ObjectVsBroadPhaseLayerFilter> objectVsBroadPhaseLayerFilter = nullptr;
            Ref<ContactListener> contactListener = nullptr;

            std::unordered_map<uint64_t, ContactPair> contactPairs;
            std::vector<JPH::Ref<JPH::TwoBodyConstraint>> constraints;

            Ref<JPH::StateRecorderImpl> state = nullptr;

            // Time step of a collision step in the last update
            float stepDeltaTime = 0.0f;

            friend class Scene::Scene;
            friend class RigidBodyComponent;
            friend class PhysicsSerializer;
            friend Constraint;

        };


    }

}