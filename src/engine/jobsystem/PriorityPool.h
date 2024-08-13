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

        void Work(int32_t workerId);

        Worker& GetNextWorker();

        std::vector<Worker>& GetAllWorkers();

        JobPriority priority;
        int32_t workerCount;

    private:
        std::vector<Worker> workers;
        std::atomic_uint32_t workerCounter = 0;
        std::atomic_bool shutdown = false;

    };

}