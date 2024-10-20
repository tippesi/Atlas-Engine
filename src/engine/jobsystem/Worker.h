#pragma once

#include "../System.h"

#include "Signal.h"
#include "ThreadSafeJobQueue.h"

#include <thread>

namespace Atlas {

    class Worker {

    public:
        Worker() = default;

        Worker(int32_t workerId, JobPriority priority);

        Worker(Worker&& worker);

        void Start(std::function<void(Worker&)> function);

        inline void Work() {

            while(true) {
                auto job = queue.Pop();
                if (job == std::nullopt)
                    break;

                RunJob(job.value());
            }

        }

        int32_t workerId;
        JobPriority priority;

        std::thread thread;
        std::atomic_bool quit = false;

        Signal signal;
        ThreadSafeJobQueue queue;

    private:
        inline void RunJob(Job& job) {

            JobData data = {
                .idx = job.idx,
                .workerIdx = workerId,
                .userData = job.userData
            };

            job.function(data);

            job.counter->fetch_sub(1);

        }

    };

}
