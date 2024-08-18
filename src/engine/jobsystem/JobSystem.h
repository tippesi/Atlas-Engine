#pragma once

/*
 * Based on ideas from:
 * - https://blog.molecular-matters.com/2015/08/24/job-system-2-0-lock-free-work-stealing-part-1-basics/
 * - https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiJobSystem.cpp
 * - https://codereview.stackexchange.com/questions/276593/c20-multi-queue-thread-pool-with-work-stealing
 */
#include "../System.h"

#include "Job.h"
#include "JobGroup.h"
#include "PriorityPool.h"

namespace Atlas {

    struct JobSystemConfig {
        int32_t highPriorityThreadCount = int32_t(std::thread::hardware_concurrency()) - 1;
        int32_t mediumPriorityThreadCount = int32_t(std::thread::hardware_concurrency()) - 3;
        int32_t lowPriorityThreadCount = int32_t(std::thread::hardware_concurrency()) - 4;
    };
    
    class JobSystem {

    public:
        static void Init(const JobSystemConfig& config);

        static void Shutdown();
        
        static void Execute(JobGroup& group, std::function<void(JobData&)> func, 
            void* userData = nullptr);

        static void ExecuteMultiple(JobGroup& group, int32_t count, 
            std::function<void(JobData&)> func, void* userData = nullptr);

        static void Wait(JobGroup& group);

        static void WaitSpin(JobGroup& group);

        static void WaitAll();
    
    private:
        static PriorityPool priorityPools[static_cast<int>(JobPriority::Count)];

    };

}