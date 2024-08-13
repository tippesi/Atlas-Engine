#pragma once

#include "../System.h"
#include "Job.h"

#include <mutex>
#include <optional>
#include <deque>

namespace Atlas {
    
    class ThreadSafeJobQueue {

    public:
        ThreadSafeJobQueue() = default;

        void Push(const Job& job);
        
        std::optional<Job> Pop();

    private:
        std::mutex mutex;
        std::deque<Job> jobs;

    };

}