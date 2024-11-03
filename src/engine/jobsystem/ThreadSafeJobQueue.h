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

        bool Empty();

        void Push(const Job& job);
        
        void PushMultiple(const std::vector<Job>& jobs);
        
        std::optional<Job> Pop();

    private:
        std::mutex mutex;
        std::deque<Job> jobs;

    };

}