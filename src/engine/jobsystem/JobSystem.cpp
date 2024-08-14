#include "JobSystem.h"
#include "Log.h"

namespace Atlas {

    PriorityPool JobSystem::priorityPools[static_cast<int>(JobPriority::Count)];

    void JobSystem::Init(const JobSystemConfig& config) {

        auto highPrioThreadCount = std::max(1, config.highPriorityThreadCount);
        auto mediumPrioThreadCount = std::max(1, config.mediumPriorityThreadCount);
        auto lowPrioThreadCount = std::max(1, config.lowPriorityThreadCount);

        priorityPools[static_cast<int>(JobPriority::High)].Init(highPrioThreadCount, JobPriority::High);
        priorityPools[static_cast<int>(JobPriority::Medium)].Init(mediumPrioThreadCount, JobPriority::Medium);
        priorityPools[static_cast<int>(JobPriority::Low)].Init(lowPrioThreadCount, JobPriority::Low);

    }

    void JobSystem::Shutdown() {

        int32_t poolCount = static_cast<int>(JobPriority::Count);
        for (int32_t i = 0; i < poolCount; i++) {
            priorityPools[i].Shutdown();
        }

    }

    void JobSystem::Execute(JobGroup& group, std::function<void(JobData&)> func, void* userData) {

        auto& priorityPool = priorityPools[static_cast<int>(group.priority)];
        group.counter.fetch_add(1);

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

            auto& worker = priorityPool.GetNextWorker();
            priorityPool.Work(worker.workerId);
        }

        while (!group.HasFinished())
            std::this_thread::yield();

    }

    void JobSystem::WaitSpin(JobGroup& group) {

        auto& priorityPool = priorityPools[static_cast<int>(group.priority)];

        auto spinCount = priorityPool.spinCounter.fetch_sub(1);

        // We can't let the thread pool run dry while everything is spinning
        if (spinCount < 1) {
            priorityPool.spinCounter.fetch_add(1);
            Wait(group);
        }
        else {
            while (!group.HasFinished());
            priorityPool.spinCounter.fetch_add(1);
        }

    }

    void JobSystem::WaitAll() {



    }

}