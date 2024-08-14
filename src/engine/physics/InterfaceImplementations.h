#pragma once

#include "../Log.h"
#include <Jolt/Jolt.h>

#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <cstdarg>

namespace Atlas {

    namespace Physics {

        using BodyID = JPH::BodyID;

        enum Layers : JPH::ObjectLayer {
            Static = 0,
            Movable = 1,
            NumLayers = 2
        };

        // Class that determines if two object layers can collide
        class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
        public:
            virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
                switch (inObject1) {
                    case Layers::Static:
                        return inObject2 == Layers::Movable; // Non moving only collides with moving
                    case Layers::Movable:
                        return true; // Moving collides with everything
                    default:
                        JPH_ASSERT(false);
                        return false;
                }
            }
        };

        // Each broadphase layer results in a separate bounding volume tree in the broad phase. You at least want to have
        // a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
        // You can have a 1-on-1 mapping between object layers and broadphase layers (like in this case) but if you have
        // many object layers you'll be creating many broad phase trees, which is not efficient. If you want to fine tune
        // your broadphase layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
        namespace BroadPhaseLayers {
            static constexpr JPH::BroadPhaseLayer Static(0);
            static constexpr JPH::BroadPhaseLayer Movable(1);
            static constexpr uint32_t NumLayers(2);
        };

        // BroadPhaseLayerInterface implementation
        // This defines a mapping between object and broadphase layers.
        class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
        public:
            BroadPhaseLayerInterfaceImpl() {
                // Create a mapping table from object to broad phase layer
                mObjectToBroadPhase[Layers::Static] = BroadPhaseLayers::Static;
                mObjectToBroadPhase[Layers::Movable] = BroadPhaseLayers::Movable;
            }

            virtual uint32_t GetNumBroadPhaseLayers() const override {
                return BroadPhaseLayers::NumLayers;
            }

            virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
                JPH_ASSERT(inLayer < Layers::NumLayers);
                return mObjectToBroadPhase[inLayer];
            }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
            virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
                switch ((JPH::BroadPhaseLayer::Type)inLayer) {
                    case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::Static:	return "STATIC";
                    case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::Movable:	return "MOVABLE";
                    default: return "INVALID";
                }
            }
#endif

        private:
            JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NumLayers];

        };

        // Class that determines if an object layer can collide with a broadphase layer
        class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
        public:
            virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
                switch (inLayer1) {
                    case Layers::Static:
                        return inLayer2 == BroadPhaseLayers::Movable;
                    case Layers::Movable:
                        return true;
                    default:
                        JPH_ASSERT(false);
                        return false;
                }
            }
        };

        class ContactListener : public JPH::ContactListener {
        public:
            virtual JPH::ValidateResult	OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult) override {
                Log::Message("Contact validation");

                // Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
                return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
            }

            virtual void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override {
                Log::Message("Contact added");
            }

            virtual void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override {
                Log::Message("Contact persisted");
            }

            virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override {
                Log::Message("Contact removed");
            }
        };

    }

}