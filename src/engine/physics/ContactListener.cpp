#include "ContactListener.h"
#include "MathConversion.h"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Collision/Shape/SubShapeIDPair.h>

namespace Atlas::Physics {

    void ContactListener::OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2,
        const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) {

        JPH::RVec3 position = JPH::RVec3::sZero();
        for (JPH::uint i = 0; i < inManifold.mRelativeContactPointsOn1.size(); i++)
            position += inManifold.GetWorldSpaceContactPointOn1(i);
        if (!inManifold.mRelativeContactPointsOn1.empty())
            position /= float(inManifold.mRelativeContactPointsOn1.size());

        auto relativeVelocity = inBody1.GetPointVelocity(position) - inBody2.GetPointVelocity(position);

        ContactEvent event;
        event.bodyId = inBody1.GetID();
        event.otherBodyId = inBody2.GetID();
        event.userData = inBody1.GetUserData();
        event.otherUserData = inBody2.GetUserData();
        event.position = JPHVecToVec(position);
        event.normal = JPHVecToVec(inManifold.mWorldSpaceNormal);
        event.impactSpeed = glm::max(0.0f, relativeVelocity.Dot(inManifold.mWorldSpaceNormal));
        event.isTrigger = inBody1.IsSensor() || inBody2.IsSensor();

        std::scoped_lock lock(mutex);
        addedContacts.push_back(event);

    }

    void ContactListener::OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) {

        std::scoped_lock lock(mutex);
        removedContacts.emplace_back(inSubShapePair.GetBody1ID(), inSubShapePair.GetBody2ID());

    }

}
