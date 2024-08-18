#include "BodyCreation.h"
#include "MathConversion.h"

namespace Atlas::Physics {

    void BodyCreationSettings::SetSettings(JPH::BodyCreationSettings settings) {

        objectLayer = settings.mObjectLayer;
        motionQuality = settings.mMotionQuality;

        linearVelocity = JPHVecToVec(settings.mLinearVelocity);
        angularVelocity = JPHVecToVec(settings.mAngularVelocity);

        friction = settings.mFriction;
        restitution = settings.mRestitution;

        linearDampening = settings.mLinearDamping;
        angularDampening = settings.mAngularDamping;

        gravityFactor = settings.mGravityFactor;

        //shapePtr = settings.GetShape();

    }

    JPH::BodyCreationSettings BodyCreationSettings::GetSettings() const {

        JPH::BodyCreationSettings settings;

        JPH::EMotionType motionType;
        switch(objectLayer) {
            case Layers::Static: motionType = JPH::EMotionType::Static; break;
            case Layers::Movable: motionType = JPH::EMotionType::Dynamic; break;
            default: motionType = JPH::EMotionType::Static; break;
        }

        settings.mObjectLayer = objectLayer;
        settings.mMotionQuality = motionQuality;
        settings.mMotionType = motionType;

        settings.mLinearVelocity = VecToJPHVec(linearVelocity);
        settings.mAngularVelocity = VecToJPHVec(angularVelocity);

        settings.mFriction = friction;
        settings.mRestitution = restitution;

        settings.mLinearDamping = linearDampening;
        settings.mAngularDamping = angularDampening;

        settings.mGravityFactor = gravityFactor;

        settings.SetShape(shape->ref);

        return settings;

    }

}