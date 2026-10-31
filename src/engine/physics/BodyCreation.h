#pragma once

#include "System.h"
#include "Shape.h"

#include "InterfaceImplementations.h"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>

namespace Atlas::Physics {

    using MotionQuality = JPH::EMotionQuality;
    using ObjectLayer = JPH::ObjectLayer;

    struct BodyCreationSettings {
        void SetSettings(JPH::BodyCreationSettings settings);

        JPH::BodyCreationSettings GetSettings() const;

        ObjectLayer objectLayer = Layers::Static;
        MotionQuality motionQuality = MotionQuality::Discrete;

        vec3 linearVelocity = vec3(0.0f);
        vec3 angularVelocity = vec3(0.0f);

        float friction = 1.0f;
        float restitution = 0.2f;

        float linearDampening = 0.05f;
        float angularDampening = 0.05f;

        float gravityFactor = 1.0f;

        // Triggers report contacts with movable bodies and players but don't collide, they never move by themselves
        bool isTrigger = false;
        // Bodies with buoyancy float in fluid triggers below their top
        bool isFluid = false;

        // 1 is neutral, lower values sink, higher values float and zero disables it
        float buoyancy = 0.0f;
        float fluidLinearDrag = 0.5f;
        float fluidAngularDrag = 0.01f;

        Ref<Shape> shape = nullptr;
    };

}