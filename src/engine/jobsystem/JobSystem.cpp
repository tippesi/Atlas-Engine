#include "JobSystem.h"
#include "Log.h"

#ifdef AE_OS_WINDOWS
#define NOMINMAX
#include "Windows.h"
#endif

#if defined(AE_OS_MACOS) || defined(AE_OS_LINUX)
#include <pthread.h>
#endif

namespace Atlas {

    PriorityPool JobSystem::priorityPools[static_cast<int>(JobPriority::Count)];

    void JobSystem::Init(const JobSystemConfig& config) {

        auto highPrioThreadCount = std::max(1, config.highPriorityThreadCount);
        auto mediumPrioThreadCount = std::max(1, config.mediumPriorityThreadCount);
        auto lowPrioThreadCount = std::max(1, config.lowPriorityThreadCount);

        priorityPools[static_cast<int>(JobPriority::High)].Init(highPrioThreadCount, JobPriority::High);
        priorityPools[static_cast<int>(JobPriority::Medium)].Init(mediumPrioThreadCount, JobPriority::Medium);
        priorityPools[static_cast<int>(JobPriority::Low)].Init(lowPrioThreadCount, JobPriority::Low);

        bool success = false;
        // Need to set our own main thread priority, otherwise we will loose when in contention with other threads
#ifdef AE_OS_WINDOWS
        auto threadHandle = GetCurrentThread();
        success = SetThreadPriority(threadHandle, THREAD_PRIORITY_HIGHEST) > 0;
#endif
#if defined(AE_OS_MACOS) || defined(AE_OS_LINUX)
        auto maxPriority = sched_get_priority_max(SCHED_RR);
        sched_param params = { 
            .sched_priority = maxPriority
        };
        success = pthread_setschedparam(pthread_self(), SCHED_RR, &params) == 0;
#endif
        if (!success)
            Log::Warning("Couldn't set priority of main thread");

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
        worker.signal.Notify();

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

        if (count <= priorityPool.workerCount) {
            for (int32_t i = 0; i < count; i++) {
                auto& worker = priorityPool.GetNextWorker();

                job.idx = i;
                worker.queue.Push(job);
                worker.signal.Notify();
            }
            return;
        }

        int32_t totalCount = 0;
        int32_t remainingJobs = count;
        int32_t jobCountPerWorker = count / priorityPool.workerCount;
        std::vector<Job> jobs;
        for (int32_t i = 0; i < priorityPool.workerCount; i++) {
            auto jobsToPush = jobCountPerWorker;
            if (i == priorityPool.workerCount - 1 && remainingJobs != jobsToPush)
                jobsToPush = remainingJobs;

            jobs.reserve(jobsToPush);

            for (int32_t j = 0; j < jobsToPush; j++) {
                job.idx = totalCount++;
                jobs.push_back(job);
            }

            auto& worker = priorityPool.GetNextWorker();
            worker.queue.PushMultiple(jobs);
            worker.signal.Notify();
            jobs.clear();

            remainingJobs -= jobsToPush;
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

    int32_t JobSystem::GetWorkerCount(const JobPriority priority) {

        auto& priorityPool = priorityPools[static_cast<int>(priority)];
        return priorityPool.workerCount;

    }

}