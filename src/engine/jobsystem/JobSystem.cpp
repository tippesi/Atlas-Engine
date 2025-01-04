#include "JobSystem.h"
#include "Log.h"

#ifdef AE_OS_WINDOWS
#define NOMINMAX
#include "Windows.h"
#endif

#if defined(AE_OS_MACOS) || defined(AE_OS_LINUX)
#include <pthread.h>
#endif

// Use this macro to make the job system single threaded
// #define JOBS_SINGLE_THREADED

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
        success = SetThreadPriority(threadHandle, THREAD_PRIORITY_TIME_CRITICAL) > 0;
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

        Job job = {
            .priority = group.priority,
            .counter = &group.counter,
            .function = std::move(func),
            .userData = userData
        };

#ifdef JOBS_SINGLE_THREADED
        auto jobData = job.GetJobData();
        job.function(jobData);
        return;
#endif

        auto& priorityPool = priorityPools[static_cast<int>(group.priority)];
        group.counter.fetch_add(1);

        auto& worker = priorityPool.GetNextWorker();
        worker.queue.Push(std::move(job));
        worker.signal.Notify();

    }

    void JobSystem::ExecuteMultiple(JobGroup& group, int32_t count, std::function<void(JobData&)> func, void* userData) {

        Job job = {
            .priority = group.priority,
            .counter = &group.counter,
            .function = std::move(func),
            .userData = userData
        };

#ifdef JOBS_SINGLE_THREADED
        for (size_t i = 0; i < count; i++) {
            job.idx = int32_t(i);
            auto jobData = job.GetJobData();
            job.function(jobData);
        }
        return;
#endif

        auto& priorityPool = priorityPools[static_cast<int>(group.priority)];
        group.counter += count;

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
                jobs.emplace_back(job);
            }

            auto& worker = priorityPool.GetNextWorker();
            worker.queue.PushMultiple(jobs);
            worker.signal.Notify();
            jobs.clear();

            remainingJobs -= jobsToPush;
        }

    }

    void JobSystem::ExecuteMultiple(JobGroup& group, ivec3 count, std::function<void(JobData&, ivec3)> func, 
        int32_t batchSize, void* userData) {

        int32_t totalSize = count.x * count.y * count.z;
        group.userCounter = 0;

        std::atomic_int32_t counter = 0;
        auto workerCount = JobSystem::GetWorkerCount(group.priority);
        JobSystem::ExecuteMultiple(group, workerCount, [&group,
            batchSize, totalSize, count, func, &counter](JobData& jobData) {
            int32_t idx = counter.fetch_add(batchSize);
            while (idx < totalSize) {
                int32_t index = idx;

                int32_t z = index / (count.x * count.y);
                index -= (count.x * count.y * z);

                int32_t y = index / count.x;
                int32_t x = index % count.x;

                func(jobData, ivec3(x, y, z));

                idx++;

                if (idx % batchSize == 0) {
                    idx = counter.fetch_add(batchSize);
                }
            }
            });

        JobSystem::Wait(group);

    }

    void JobSystem::ParallelFor(JobGroup& group, int32_t count, int32_t jobCount, 
        std::function<void(JobData&, int32_t)> func, void* userData) {

        if (!count || !jobCount)
            return;

        int32_t iterationsPerJob = count / jobCount;
        if (count < jobCount) {
            jobCount = 1;
            iterationsPerJob = count;
        }
        else {
            iterationsPerJob = count % jobCount == 0 ? iterationsPerJob : iterationsPerJob + 1;
        }
        JobSystem::ExecuteMultiple(group, jobCount, [iterationsPerJob, count, func](JobData& data) mutable {

            int32_t offset = data.idx * iterationsPerJob;
            for (int32_t i = offset; i < iterationsPerJob + offset && i < count; i++) {
                func(data, i);
            }
            });

    }

    void JobSystem::Wait(JobSignal& signal, JobPriority priority) {

#ifdef JOBS_SINGLE_THREADED
        return;
#endif

        auto& priorityPool = priorityPools[static_cast<int>(priority)];        

        while (!signal.TryAquire()) {
            auto& worker = priorityPool.GetNextWorker();
            priorityPool.Work(worker.workerId);
        }

    }

    void JobSystem::Wait(JobGroup& group) {

#ifdef JOBS_SINGLE_THREADED
        return;
#endif

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

        bool jobsFound = true;
        while (jobsFound) {
            jobsFound = false;

            // Check if any pool has work and help pools that still have work
            for (auto& priorityPool : priorityPools) {
                bool poolHasWork = false, poolIsWorking = false;

                for (auto& worker : priorityPool.GetAllWorkers()) {
                    poolHasWork |= !worker.queue.Empty();
                    poolIsWorking |= !worker.idling;
                }

                // Help pool
                if (poolHasWork) {
                    const auto& worker = priorityPool.GetNextWorker();
                    priorityPool.Work(worker.workerId);                    
                }

                if (poolHasWork || poolIsWorking)
                    jobsFound = true;
            }
        }

    }

    int32_t JobSystem::GetWorkerCount(const JobPriority priority) {

        const auto& priorityPool = priorityPools[static_cast<int>(priority)];
        return priorityPool.workerCount;

    }

}