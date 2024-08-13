#include "ThreadSafeJobQueue.h"

namespace Atlas {

    void ThreadSafeJobQueue::Push(const Job& job) {

        std::scoped_lock lock(mutex);
        jobs.push_back(job);
    
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