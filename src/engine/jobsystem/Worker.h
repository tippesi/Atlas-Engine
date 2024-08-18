#pragma once

#include "../System.h"

#include "ThreadSafeJobQueue.h"

#include <thread>
#include <semaphore>

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
        std::binary_semaphore semaphore{0};
        ThreadSafeJobQueue queue;

    private:
        inline void RunJob(Job& job) {

            JobData data = {
                .idx = job.idx,
                .userData = job.userData
            };

            job.function(data);

            job.counter->fetch_sub(1);

        }

    };

}