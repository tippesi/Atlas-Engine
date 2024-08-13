#pragma once

#include "../System.h"

#include "ThreadSafeJobQueue.h"

#include <thread>
#include <semaphore>

namespace Atlas {

    class Worker {

    public:
        Worker() = default;

        Worker(int32_t workerId);

        Worker(Worker&& worker);

        void Start(std::function<void(Worker&)> function);

        void Work();

        int32_t workerId;

        std::thread thread;
        std::binary_semaphore semaphore{0};
        ThreadSafeJobQueue queue;

    private:
        void RunJob(Job& job);

    };

}