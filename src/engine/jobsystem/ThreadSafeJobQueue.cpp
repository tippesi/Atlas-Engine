#include "ThreadSafeJobQueue.h"

namespace Atlas {

    bool ThreadSafeJobQueue::Empty() {

        std::scoped_lock lock(mutex);
        return jobs.empty();

    }

    void ThreadSafeJobQueue::Push(const Job& job) {

        std::scoped_lock lock(mutex);
        jobs.push_back(job);
    
    }

    void ThreadSafeJobQueue::PushMultiple(const std::vector<Job>& newJobs) {

        std::scoped_lock lock(mutex);
        jobs.insert(jobs.end(), newJobs.begin(), newJobs.end());

    }

    std::optional<Job> ThreadSafeJobQueue::Pop() {

        std::scoped_lock lock(mutex);
        if (jobs.empty())
            return std::nullopt;
            
        auto job = jobs.front();
        jobs.pop_front();

        return job;

    } 

}