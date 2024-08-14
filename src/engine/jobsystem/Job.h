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
        void* userData = nullptr;
    };

    struct Job {
        int32_t idx = 0;
        JobPriority priority = JobPriority::Low;

        std::atomic_int32_t* counter = 0;
        std::function<void(JobData&)> function;

        void* userData = nullptr;
    };

}