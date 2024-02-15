#pragma once

#include "../System.h"

#include "PhysicsWorld.h"

#include <Jolt/Core/JobSystemThreadPool.h>
#include <mutex>

namespace Atlas {

    namespace Physics {

        class PhysicsManager {

        public:
            static void Init();

            static void Shutdown();

            static void ExecuteUpdate(PhysicsWorld* physicsWorld, float deltaTime);

            static Ref<JPH::TempAllocatorImpl> tempAllocator;
            static Ref<JPH::JobSystemThreadPool> jobSystemThreadPool;

        private:
            static std::mutex updateMutex;

        };

    }

}