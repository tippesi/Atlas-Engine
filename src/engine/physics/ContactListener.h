#pragma once

#include "../System.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ContactListener.h>

#include <mutex>
#include <vector>

namespace Atlas::Physics {

    using BodyID = JPH::BodyID;

    enum class ContactEventType {
        Begin = 0,
        End
    };

    struct ContactEvent {
        ContactEventType type = ContactEventType::Begin;

        BodyID bodyId;
        BodyID otherBodyId;

        uint64_t userData = 0;
        uint64_t otherUserData = 0;

        // Contact position and normal pointing from the body to the other body, both are zero for end events
        vec3 position = vec3(0.0f);
        vec3 normal = vec3(0.0f);

        // Speed with which the bodies approached each other along the normal
        float impactSpeed = 0.0f;

        bool isTrigger = false;
    };

    // Collects the contacts of a simulation step, the callbacks are called from multiple threads
    class ContactListener : public JPH::ContactListener {

    public:
        void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2,
            const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;

        void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override;

        std::vector<ContactEvent> addedContacts;
        std::vector<std::pair<BodyID, BodyID>> removedContacts;

        std::mutex mutex;

    };

}
