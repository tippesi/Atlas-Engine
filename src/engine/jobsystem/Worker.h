#pragma once

#include "../System.h"

#include "Signal.h"
#include "JobGroup.h"
#include "ThreadSafeJobQueue.h"
#include "../profiler/CPUProfiler.h"

#include <thread>

namespace Atlas {

    class Worker {

    public:
        Worker() = default;

        Worker(int32_t workerId, JobPriority priority);

        Worker(Worker&& worker);

        void Start(std::function<void(Worker&)> function);

        // Can be called from any thread (e.g. work stealing), returns the number of executed jobs.
        // Stops early once the wait group has finished
        inline int32_t Work(const JobGroup* waitGroup = nullptr) {

            int32_t executedJobs = 0;
            while(!waitGroup || !waitGroup->HasFinished()) {
                auto job = queue.Pop();
                if (job == std::nullopt)
                    break;

                RunJob(job.value());
                executedJobs++;
            }

            return executedJobs;

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

            {
                CPUProfiler::Scope profilerScope(job.name ? job.name : "Job");
                job.function(data);
            }

            job.counter->fetch_sub(1);

        }

    };

}
