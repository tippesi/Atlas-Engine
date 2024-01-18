#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>

namespace Atlas {

    namespace Physics {

        namespace Layers {
            static constexpr JPH::ObjectLayer STATIC = 0;
            static constexpr JPH::ObjectLayer MOVABLE = 1;
            static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
        };

        // Class that determines if two object layers can collide
        class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
        public:
            virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
                switch (inObject1) {
                    case Layers::STATIC:
                        return inObject2 == Layers::MOVABLE; // Non moving only collides with moving
                    case Layers::MOVABLE:
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
            static constexpr JPH::BroadPhaseLayer STATIC(0);
            static constexpr JPH::BroadPhaseLayer MOVABLE(1);
            static constexpr uint32_t NUM_LAYERS(2);
        };

        // BroadPhaseLayerInterface implementation
        // This defines a mapping between object and broadphase layers.
        class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
        public:
            BroadPhaseLayerInterfaceImpl() {
                // Create a mapping table from object to broad phase layer
                mObjectToBroadPhase[Layers::STATIC] = BroadPhaseLayers::STATIC;
                mObjectToBroadPhase[Layers::MOVABLE] = BroadPhaseLayers::MOVABLE;
            }

            virtual uint32_t GetNumBroadPhaseLayers() const override {
                return BroadPhaseLayers::NUM_LAYERS;
            }

            virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
                JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
                return mObjectToBroadPhase[inLayer];
            }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
            virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
                switch ((JPH::BroadPhaseLayer::Type)inLayer) {
                    case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::STATIC:	return "STATIC";
                    case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVABLE:	return "MOVABLE";
                    default: return "INVALID";
                }
            }
#endif

        private:
            JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];

        };

        // Class that determines if an object layer can collide with a broadphase layer
        class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
        public:
            virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
                switch (inLayer1) {
                    case Layers::STATIC:
                        return inLayer2 == BroadPhaseLayers::MOVABLE;
                    case Layers::MOVABLE:
                        return true;
                    default:
                        JPH_ASSERT(false);
                        return false;
                }
            }
        };

    }

}