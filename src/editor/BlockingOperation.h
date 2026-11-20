#pragma once

#include "jobsystem/JobSystem.h"

#include <string>
#include <vector>
#include <atomic>

namespace Atlas::Editor {

    class BlockingOperation {

    public:
        ~BlockingOperation() { JobSystem::Wait(job); }

        void Block(const std::string& text, std::function<void()> blocker) {
            blockers.push_back(blocker);
            blockText = text;
        }

        void Update() {
            if (blockers.empty() || !job.HasFinished())
                return;

            block = true;
            JobSystem::Execute(job, [blockers = std::move(blockers), &block = block](JobData&) {
                for (const auto& blocker : blockers)
                    blocker();
                block = false;
                });
            blockers.clear();
        }

        // For callers which destroy what the operations reference before the next update
        void Finish() {
            JobSystem::Wait(job);
            for (const auto& blocker : blockers)
                blocker();
            blockers.clear();
        }

        JobGroup job { "Blocking operation" };
        std::vector<std::function<void()>> blockers;
        std::string blockText;
        std::atomic_bool block = false;

    };

}
