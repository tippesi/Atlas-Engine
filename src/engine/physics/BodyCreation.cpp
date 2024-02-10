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

        shapePtr = settings.GetShape();

    }

    JPH::BodyCreationSettings BodyCreationSettings::GetSettings() const {

        JPH::BodyCreationSettings settings;

        JPH::EMotionType motionType;
        switch(objectLayer) {
            case Layers::STATIC: motionType = JPH::EMotionType::Static; break;
            case Layers::MOVABLE: motionType = JPH::EMotionType::Dynamic; break;
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

        settings.SetShape(shape->ref);

        return settings;

    }

}