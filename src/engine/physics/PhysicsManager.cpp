#include "PhysicsManager.h"

#include <Jolt/Jolt.h>

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>

namespace Atlas {

    namespace Physics {

        using namespace JPH;

        Ref<TempAllocatorImpl> PhysicsManager::tempAllocator = nullptr;
        Ref<JoltJobSystem> PhysicsManager::jobSystem = nullptr;

        std::mutex PhysicsManager::updateMutex;

        void PhysicsManager::Init() {

            RegisterDefaultAllocator();

            // Trace = TraceImpl;

            // Create a factory
            Factory::sInstance = new Factory();

            // Register all Jolt physics types
            RegisterTypes();

            tempAllocator = Atlas::CreateRef<TempAllocatorImpl>(100 * 1024 * 1024);
            jobSystem = Atlas::CreateRef<JoltJobSystem>(cMaxPhysicsJobs, cMaxPhysicsBarriers);

        }

        void PhysicsManager::Shutdown() {

            UnregisterTypes();

            // Destroy the factory
            delete Factory::sInstance;
            Factory::sInstance = nullptr;

            // Do an explicit release on shutdown
            tempAllocator.reset();
            jobSystem.reset();

        }

        void PhysicsManager::ExecuteUpdate(PhysicsWorld* physicsWorld, float deltaTime, int32_t collisionSteps) {

            std::scoped_lock<std::mutex> lock(updateMutex);

            physicsWorld->system->Update(deltaTime, collisionSteps, tempAllocator.get(), jobSystem.get());

        }

    }

}