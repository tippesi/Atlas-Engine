#include "JoltJobSystem.h"
#include "../jobsystem/JobSystem.h"

#include <thread>

namespace Atlas::Physics {

    JoltJobSystem::JoltJobSystem(uint32_t maxJobs, uint32_t maxBarriers) {

        JobSystemWithBarrier::Init(maxBarriers);
        jobs.Init(maxJobs, maxJobs);

    }

    int JoltJobSystem::GetMaxConcurrency() const {

        // The thread waiting for the physics update executes jobs as well. Jolt splits its work by this number,
        // more than Jolt's own pool used (13) only adds synchronization
        return std::min(Atlas::JobSystem::GetWorkerCount(JobPriority::High) + 1, 13);

    }

    JoltJobSystem::JobHandle JoltJobSystem::CreateJob(const char* name, JPH::ColorArg color, const JobFunction& function,
        JPH::uint32 dependencyCount) {

        auto index = jobs.ConstructObject(name, color, this, function, dependencyCount);
        while (index == JPH::FixedSizeFreeList<Job>::cInvalidObjectIndex) {
            AE_ASSERT(false && "Too many physics jobs");
            std::this_thread::yield();
            index = jobs.ConstructObject(name, color, this, function, dependencyCount);
        }

        auto job = &jobs.Get(index);
        JobHandle handle(job);
        if (dependencyCount == 0)
            QueueJob(job);

        return handle;

    }

    void JoltJobSystem::QueueJob(Job* job) {

        // Jolt keeps the job alive only during this call
        job->AddRef();
        Atlas::JobSystem::Execute(jobGroup, [job](JobData&) {
            job->Execute();
            job->Release();
            });

    }

    void JoltJobSystem::QueueJobs(Job** jobs, JPH::uint count) {

        for (JPH::uint i = 0; i < count; i++)
            QueueJob(jobs[i]);

    }

    void JoltJobSystem::FreeJob(Job* job) {

        jobs.DestructObject(job);

    }

}
