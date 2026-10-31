#pragma once

#include "../System.h"
#include "../volume/Ray.h"
#include "../volume/AABB.h"
#include "ShapesManager.h"
#include "BodyCreation.h"
#include "Body.h"
#include "Player.h"
#include "Constraint.h"
#include "ContactListener.h"

#include "InterfaceImplementations.h"
#include <Jolt/Physics/StateRecorderImpl.h>
#include <Jolt/Physics/PhysicsStepListener.h>

#include <unordered_set>

namespace Atlas {

    namespace Renderer {

        class PrimitiveBatch;

    }

    namespace Scene {

        class Scene;

        namespace Components {

            class RigidBodyComponent;

        }

    }

    namespace Physics {

        class BuoyancyStepListener;

        // Endless fluid below the surface, bodies with buoyancy float in it
        struct Fluid {
            vec3 surfacePosition = vec3(0.0f);
            vec3 surfaceNormal = vec3(0.0f, 1.0f, 0.0f);
            vec3 velocity = vec3(0.0f);
        };

        struct DebugDrawSettings {
            bool drawShapes = true;
            bool drawBounds = false;
            bool drawContacts = true;
            bool drawConstraints = true;
            bool drawStaticBodies = true;

            // Only what is within the box of the distance around the center is drawn
            vec3 center = vec3(0.0f);
            float distance = 50.0f;
        };

        struct WorldStatistics {
            uint32_t bodyCount = 0;
            uint32_t staticBodyCount = 0;
            uint32_t dynamicBodyCount = 0;
            uint32_t activeDynamicBodyCount = 0;
            uint32_t kinematicBodyCount = 0;
            uint32_t activeKinematicBodyCount = 0;
            uint32_t constraintCount = 0;
            uint32_t contactCount = 0;

            // Time of the last update in milliseconds
            float updateTime = 0.0f;
        };

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

            bool HasBuoyancy(BodyID bodyId) const;

            Volume::AABB GetBodyBounds(BodyID bodyId) const;

            uint32_t GetBodyCount() const;

            WorldStatistics GetStatistics() const;

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

            // Draws the bodies, contacts and constraints with lines, contacts are drawn from the next update on
            void DebugDraw(Renderer::PrimitiveBatch* primitiveBatch, const DebugDrawSettings& settings);

            void OptimizeBroadphase();

            void SaveState();

            void RestoreState();

            Ref<JPH::PhysicsSystem> system = nullptr;

            int32_t simulationStepsPerSecond = 60;
            bool pauseSimulation = false;

            std::unordered_map<BodyID, Ref<Shape>> bodyToShapeMap;

            // Contacts that began and ended since the events were last cleared
            std::vector<ContactEvent> contactEvents;

            // Fluids in addition to the fluid triggers
            std::vector<Fluid> fluids;
            // Fluids of single bodies that replace the fluids above, the scene sets the ocean waves here every update
            std::unordered_map<BodyID, Fluid> bodyFluids;

        private:
            struct ContactPair {
                ContactEvent event;
                int32_t count = 0;
                bool sleeping = false;
            };

            struct Buoyancy {
                float buoyancy = 0.0f;
                float linearDrag = 0.0f;
                float angularDrag = 0.0f;
            };

            void ProcessContacts();

            void FindFluidContacts();

            // Called before every collision step on a job thread
            void ApplyBuoyancy(float deltaTime);

            void EndContact(const ContactPair& contactPair);

            Ref<JPH::ObjectLayerPairFilter> objectLayerFilter = nullptr;
            Ref<JPH::BroadPhaseLayerInterface> broadPhaseLayerInterface = nullptr;
            Ref<JPH::ObjectVsBroadPhaseLayerFilter> objectVsBroadPhaseLayerFilter = nullptr;
            Ref<ContactListener> contactListener = nullptr;

            Ref<JPH::PhysicsStepListener> stepListener = nullptr;

            std::unordered_map<uint64_t, ContactPair> contactPairs;
            std::vector<JPH::Ref<JPH::TwoBodyConstraint>> constraints;

            std::unordered_map<BodyID, Buoyancy> bodyToBuoyancyMap;
            std::unordered_set<BodyID> fluidBodies;
            // Fluid triggers that touched the bodies with buoyancy in the last update
            std::unordered_map<BodyID, std::vector<BodyID>> bodyToFluidBodiesMap;

            Ref<JPH::StateRecorderImpl> state = nullptr;

            // Time step of a collision step in the last update
            float stepDeltaTime = 0.0f;
            float updateTime = 0.0f;

            friend class Scene::Scene;
            friend class RigidBodyComponent;
            friend class PhysicsSerializer;
            friend Constraint;
            friend BuoyancyStepListener;

        };


    }

}