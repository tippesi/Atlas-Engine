#pragma once

#include "../System.h"
#include "Job.h"

#include <atomic>

namespace Atlas {

    class JobGroup {

    public:
        JobGroup() = default;

        JobGroup(JobPriority priority);

        JobGroup(const char* name, JobPriority priority = JobPriority::Low);

        ~JobGroup();

        bool HasFinished();

        const char* name = nullptr;

        JobPriority priority = JobPriority::Low;
        std::atomic_int32_t counter = 0;
        std::atomic_int32_t userCounter = 0;

    };

}