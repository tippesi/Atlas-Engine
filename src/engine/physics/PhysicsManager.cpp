#include "PhysicsManager.h"

#include <Jolt/Jolt.h>

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>

namespace Atlas {

    namespace Physics {

        using namespace JPH;

        Ref<TempAllocatorImpl> PhysicsManager::tempAllocator = nullptr;
        Ref<JobSystemThreadPool> PhysicsManager::jobSystemThreadPool = nullptr;

        std::mutex PhysicsManager::updateMutex;

        void PhysicsManager::Init() {

            RegisterDefaultAllocator();

            // Create a factory
            Factory::sInstance = new Factory();

            // Register all Jolt physics types
            RegisterTypes();

            tempAllocator = Atlas::CreateRef<TempAllocatorImpl>(100 * 1024 * 1024);
            jobSystemThreadPool = Atlas::CreateRef<JobSystemThreadPool>(cMaxPhysicsJobs,
                cMaxPhysicsBarriers, thread::hardware_concurrency() / 2);

        }

        void PhysicsManager::Shutdown() {

            UnregisterTypes();

            // Destroy the factory
            delete Factory::sInstance;
            Factory::sInstance = nullptr;

            // Do an explicit release on shutdown
            tempAllocator.reset();
            jobSystemThreadPool.reset();

        }

        void PhysicsManager::ExecuteUpdate(PhysicsWorld* physicsWorld, float deltaTime) {

            std::scoped_lock<std::mutex> lock(updateMutex);

            uint32_t collisionSteps = std::min(std::max(uint32_t(float(physicsWorld->simulationStepsPerSecond) * deltaTime), 1u), 8u);
            physicsWorld->system.Update(deltaTime, collisionSteps, tempAllocator.get(), jobSystemThreadPool.get());

        }

    }

}