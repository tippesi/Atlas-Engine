#pragma once

#include "../System.h"

#include "PhysicsWorld.h"
#include "JoltJobSystem.h"

#include <mutex>

namespace Atlas {

    namespace Physics {

        class PhysicsManager {

        public:
            static void Init();

            static void Shutdown();

            static void ExecuteUpdate(PhysicsWorld* physicsWorld, float deltaTime, int32_t collisionSteps);

            static Ref<JPH::TempAllocatorImpl> tempAllocator;
            static Ref<JoltJobSystem> jobSystem;

        private:
            static std::mutex updateMutex;

        };

    }

}