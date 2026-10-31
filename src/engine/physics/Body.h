#pragma once

#include "InterfaceImplementations.h"
#include "BodyCreation.h"

namespace Atlas::Physics {

    class PhysicsWorld;

    class Body {

    public:
        Body() = default;
        Body(BodyID bodyId, PhysicsWorld* world) : bodyId(bodyId), world(world) {}

        bool IsValid() const { return world != nullptr && !bodyId.IsInvalid(); }

        void SetMatrix(mat4 matrix);

        void MoveKinematic(mat4 matrix, float deltaTime);

        mat4 GetMatrix();

        void SetMotionQuality(MotionQuality quality);

        MotionQuality GetMotionQuality();

        void SetLinearVelocity(vec3 velocity);

        vec3 GetLinearVelocity();

        void SetAngularVelocity(vec3 velocity);

        vec3 GetAngularVelocity();

        void SetRestitution(float restitution);

        float GetRestitution();

        void SetFriction(float friction);

        float GetFriction();

        void AddForce(vec3 force);

        void AddForce(vec3 force, vec3 position);

        void AddImpulse(vec3 impulse);

        void AddImpulse(vec3 impulse, vec3 position);

        void AddTorque(vec3 torque);

        void AddAngularImpulse(vec3 angularImpulse);

        Ref<Shape> GetShape() const;

        uint64_t GetUserData() const;

        virtual BodyCreationSettings GetBodyCreationSettings() const;

        BodyID bodyId;

    protected:
        PhysicsWorld* world = nullptr;

        friend PhysicsWorld;

    };

}