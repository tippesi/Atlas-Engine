#pragma once

#include "../System.h"
#include "../jobsystem/JobGroup.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemWithBarrier.h>
#include <Jolt/Core/FixedSizeFreeList.h>

namespace Atlas::Physics {

    // Runs the jobs of Jolt on the high priority workers of the engine instead of a separate thread pool
    class JoltJobSystem final : public JPH::JobSystemWithBarrier {

    public:
        JoltJobSystem(uint32_t maxJobs, uint32_t maxBarriers);

        int GetMaxConcurrency() const override;

        JobHandle CreateJob(const char* name, JPH::ColorArg color, const JobFunction& function, JPH::uint32 dependencyCount = 0) override;

    protected:
        void QueueJob(Job* job) override;

        void QueueJobs(Job** jobs, JPH::uint count) override;

        void FreeJob(Job* job) override;

    private:
        JPH::FixedSizeFreeList<Job> jobs;
        JobGroup jobGroup{ "Physics", JobPriority::High };

    };

}
