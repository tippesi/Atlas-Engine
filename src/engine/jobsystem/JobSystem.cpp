#include "JobSystem.h"

namespace Atlas {

    PriorityPool JobSystem::priorityPools[static_cast<int>(JobPriority::Count)];

    void JobSystem::Init(const JobSystemConfig& config) {

        int32_t lowPriorityWorkerCount = std::max(1, int32_t(float(config.threadCount) * config.lowPriorityPercentage));
        int32_t highPriorityWorkerCount = config.threadCount - lowPriorityWorkerCount;

        priorityPools[static_cast<int>(JobPriority::High)].Init(highPriorityWorkerCount, JobPriority::High);
        priorityPools[static_cast<int>(JobPriority::Low)].Init(lowPriorityWorkerCount, JobPriority::Low);

    }

    void JobSystem::Shutdown() {

        int32_t poolCount = static_cast<int>(JobPriority::Count);
        for (int32_t i = 0; i < poolCount; i++) {
            priorityPools[i].Shutdown();
        }

    }

    void JobSystem::Execute(JobGroup& group, std::function<void(JobData&)> func, void* userData) {

        auto& priorityPool = priorityPools[static_cast<int>(group.priority)];
        group.counter++;

        Job job = {
            .priority = group.priority,
            .counter = &group.counter,
            .function = func,
            .userData = userData
        };

        auto& worker = priorityPool.GetNextWorker();
        worker.queue.Push(job);
        worker.semaphore.release();        

    }

    void JobSystem::ExecuteMultiple(JobGroup& group, int32_t count, std::function<void(JobData&)> func, void* userData) {

        auto& priorityPool = priorityPools[static_cast<int>(group.priority)];
        group.counter += count;

         Job job = {
            .priority = group.priority,
            .counter = &group.counter,
            .function = func,
            .userData = userData
        };

        // Not very efficient, could just batch push back all this work
        for (int32_t i = 0; i < count; i++) {
            auto& worker = priorityPool.GetNextWorker();

            job.idx = i;
            worker.queue.Push(job);
        }

        auto& workers = priorityPool.GetAllWorkers();
        for (auto& worker : workers) {
            worker.semaphore.release();
        }

    }

    void JobSystem::Wait(JobGroup& group) {

        if (!group.HasFinished()) {
            auto& priorityPool = priorityPools[static_cast<int>(group.priority)];


        }

        while (!group.HasFinished());

    }

    void JobSystem::WaitSpin(JobGroup& group) {

        while (!group.HasFinished());

    }

    void JobSystem::WaitAll() {



    }

}