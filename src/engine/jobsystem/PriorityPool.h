#pragma once

#include "../System.h"
#include "Job.h"
#include "Worker.h"

#include <vector>

namespace Atlas {
    
    class PriorityPool {

    public:
        void Init(int32_t workerCount, JobPriority priority);

        void Shutdown();

        Worker& GetNextWorker();

        inline void Work(int32_t workerId) {

            auto& worker = workers[workerId];
            auto executedJobs = worker.Work();

            for (int32_t i = 1; i < workerCount; i++) {
                auto stealIdx = (-i + workerId + workerCount) % workerCount;
                auto& stealFrom = workers[stealIdx];
                executedJobs += stealFrom.Work();
            }

            // Only touch the shared counter once per pass instead of once per job
            if (executedJobs > 0)
                pendingJobs.fetch_sub(executedJobs);

        }

        JobPriority priority;
        int32_t workerCount;
        std::atomic_uint32_t spinCounter = 0;

        // Jobs which are queued or still executing. Has its own cache line, since it's written by all workers.
        alignas(64) std::atomic_int32_t pendingJobs = 0;

    private:
        std::vector<Worker> workers;
        std::atomic_uint32_t workerCounter = 0;
        std::atomic_bool shutdown = false;

    };

}