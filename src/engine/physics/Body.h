#pragma once

#include "InterfaceImplementations.h"
#include "BodyCreation.h"

namespace Atlas::Physics {

    class PhysicsWorld;

    class Body {

    public:
        Body() = default;
        Body(BodyID bodyId, PhysicsWorld* world) : bodyId(bodyId), world(world) {}

        bool IsValid() const { return world != nullptr; }

        void SetMatrix(mat4 matrix);

        mat4 GetMatrix();

        void SetMotionQuality(MotionQuality quality);

        MotionQuality GetMotionQuality();

        void SetLinearVelocity(vec3 velocity);

        vec3 GetLinearVelocity();

        void SetRestitution(float restitution);

        float GetRestitution();

        void SetFriction(float friction);

        float GetFriction();

        uint64_t GetUserData() const;

        virtual BodyCreationSettings GetBodyCreationSettings();

        BodyID bodyId;

    protected:
        PhysicsWorld* world = nullptr;

        friend PhysicsWorld;

    };

}