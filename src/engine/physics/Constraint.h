#pragma once

#include "../System.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>

namespace Atlas::Physics {

    class PhysicsWorld;

    // Values are persisted in scenes, append new types at the end
    enum class ConstraintType {
        Fixed = 0,
        Point,
        Hinge,
        Slider,
        Distance,
        Cone
    };

    // Points and axes are in the local space of the first body, the other point in the space of the other body
    // or in world space without another body
    struct ConstraintCreationSettings {
        ConstraintType type = ConstraintType::Fixed;

        vec3 point = vec3(0.0f);
        vec3 otherPoint = vec3(0.0f);
        vec3 axis = vec3(0.0f, 1.0f, 0.0f);

        // Hinge angles (radians), slider positions or distances
        bool enableLimits = false;
        float limitsMin = 0.0f;
        float limitsMax = 0.0f;

        float halfConeAngle = 0.5f;

        // Force at which the constraint breaks, zero never breaks
        float breakForce = 0.0f;
    };

    class Constraint {

    public:
        Constraint() = default;
        Constraint(JPH::Ref<JPH::TwoBodyConstraint> ref, PhysicsWorld* world) : ref(ref), world(world) {}

        // Constraints are destroyed together with their bodies
        bool IsValid() const;

        void SetEnabled(bool enabled);

        bool IsEnabled() const;

        // Force that held the bodies together in the last simulation step
        float GetForce() const;

        JPH::Ref<JPH::TwoBodyConstraint> ref = nullptr;

    protected:
        PhysicsWorld* world = nullptr;

        friend PhysicsWorld;

    };

}
