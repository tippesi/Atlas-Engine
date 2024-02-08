#pragma once

#include "common/SerializationHelper.h"
#include "PhysicsWorld.h"

#include <string>

#include <Jolt/Physics/PhysicsScene.h>
#include <Jolt/ObjectStream/ObjectStreamTextIn.h>
#include <Jolt/ObjectStream/ObjectStreamTextOut.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>

namespace Atlas::Physics {

    void SerializePhysicsSystem(std::stringstream& stream, const PhysicsWorld& physicsWorld);

    void DeserializePhysicsSystem(std::stringstream& stream, PhysicsWorld& physicsWorld);

    void to_json(json& j, const PhysicsWorld& p) {
        std::stringstream stream;
        SerializePhysicsSystem(stream, p);

        j = json {
            {"system", stream.str()},
        };
    }

    void from_json(const json& j, PhysicsWorld& p) {
        p = PhysicsWorld();

        std::stringstream stream(j["system"].get<std::string>());
        DeserializePhysicsSystem(stream, p);
    }

    void SerializePhysicsSystem(std::stringstream& stream, const PhysicsWorld& physicsWorld) {

        /*
        JPH::PhysicsScene scene;

        auto& system = physicsWorld.system;
        auto& bodyInterface = system->GetBodyInterface();

        JPH::BodyIDVector bodyIds;
        system->GetBodies(bodyIds);

        auto& bodyLockInterface = system->GetBodyLockInterface();

        std::set<const JPH::Shape*> shapes;

        for (auto bodyId : bodyIds) {
            JPH::BodyLockRead lock(bodyLockInterface, bodyId);
            if (lock.Succeeded()) {
                const auto& body = lock.GetBody();

                if (body.IsRigidBody()) {
                    JPH::BodyCreationSettings settings = body.GetBodyCreationSettings();
                    std::memcpy(&settings.mUserData, &bodyId, sizeof(JPH::BodyID));
                    scene.AddBody(settings);
                    if (!shapes.contains(body.GetShape())) {
                        shapes.insert(body.GetShape());
                    }
                }
                else {
                    JPH::SoftBodyCreationSettings settings = body.GetSoftBodyCreationSettings();
                    std::memcpy(&settings.mUserData, &bodyId, sizeof(JPH::BodyID));
                    scene.AddSoftBody(settings);
                }
            }
        }

        // bodyInterface->

        JPH::ObjectStreamOut::sWriteObject(stream, JPH::ObjectStream::EStreamType::Text, scene);
         */

    }

    void DeserializePhysicsSystem(std::stringstream& stream, PhysicsWorld& physicsWorld) {

        /*
        JPH::Ref<JPH::PhysicsScene> scene;
        JPH::ObjectStreamIn::sReadObject(stream, scene);

        auto& system = physicsWorld.system;
        auto& bodyInterface = system->GetBodyInterface();

        for (auto& bodySettings : scene->GetBodies()) {
            JPH::BodyID bodyId;
            std::memcpy(&bodyId, &bodySettings.mUserData, sizeof(JPH::BodyID));
            bodyInterface.CreateBodyWithID(bodyId, bodySettings);
        }

        for (auto& bodySettings : scene->GetSoftBodies()) {
            JPH::BodyID bodyId;
            std::memcpy(&bodyId, &bodySettings.mUserData, sizeof(JPH::BodyID));
            bodyInterface.CreateSoftBodyWithID(bodyId, bodySettings);
        }
         */

    }


}