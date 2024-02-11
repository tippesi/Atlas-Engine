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

        ObjectLayer objectLayer = Layers::STATIC;
        MotionQuality motionQuality = MotionQuality::Discrete;

        vec3 linearVelocity = vec3(0.0f);
        vec3 angularVelocity = vec3(0.0f);

        float friction = 1.0f;
        float restitution = 0.2f;

        float linearDampening = 0.05f;
        float angularDampening = 0.05f;

        float gravityFactor = 1.0f;

        const JPH::Shape* shapePtr = nullptr;
        Ref<Shape> shape = nullptr;
    };

}