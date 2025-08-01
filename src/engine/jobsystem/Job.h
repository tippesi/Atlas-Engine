#pragma once

#include "../System.h"

#include <atomic>
#include <functional>

namespace Atlas {

    enum class JobPriority {
        High = 0,
        Medium,
        Low,
        Count
    };

    struct JobData {
        int32_t idx = 0;
        int32_t workerIdx = 0;
        JobPriority priority;

        void* userData = nullptr;
    };

    struct Job {
        int32_t idx = 0;
        JobPriority priority = JobPriority::Low;

        std::atomic_int32_t* counter = 0;
        std::function<void(JobData&)> function;

        void* userData = nullptr;
        const char* name = nullptr;

        inline JobData GetJobData(int32_t workerIdx = 0) const {
            return JobData {
                .idx = idx,
                .workerIdx = 0,
                .priority = priority,
                .userData = userData
            };
        }
    };

}