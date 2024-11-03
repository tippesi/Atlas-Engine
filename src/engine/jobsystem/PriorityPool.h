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

        std::vector<Worker>& GetAllWorkers();

        inline void Work(int32_t workerId) {

            auto& worker = workers[workerId];
            worker.Work();

            for (int32_t i = 1; i < workerCount; i++) {
                auto stealIdx = (-i + workerId + workerCount) % workerCount;
                auto& stealFrom = workers[stealIdx];
                stealFrom.Work();
            }

        }        

        JobPriority priority;
        int32_t workerCount;
        std::atomic_uint32_t spinCounter = 0;

    private:
        std::vector<Worker> workers;
        std::atomic_uint32_t workerCounter = 0;
        std::atomic_bool shutdown = false;

    };

}