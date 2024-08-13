#pragma once

#include "../System.h"
#include "Job.h"

#include <atomic>

namespace Atlas {

    class JobGroup {

    public:
        JobGroup() = default;

        JobGroup(JobPriority priority);

        ~JobGroup();

        bool HasFinished();

        JobPriority priority = JobPriority::Low;
        std::atomic_int32_t counter = 0;

    };

}