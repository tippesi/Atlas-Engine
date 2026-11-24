#pragma once

#include "jobsystem/JobSystem.h"

#include <functional>
#include <string>
#include <vector>

namespace Atlas::Editor {

    class BlockingOperation {

    public:
        ~BlockingOperation() { JobSystem::Wait(job); }

        void Block(const std::string& text, std::function<void()> blocker) {
            blockers.push_back(blocker);
            blockText = text;
        }

        void Update() {
            if (!job.HasFinished())
                return;

            // The frame the operation finishes in has updated nothing, so it isn't rendered either
            block = !blockers.empty();
            if (!block)
                return;

            JobSystem::Execute(job, [blockers = std::move(blockers)](JobData&) {
                for (const auto& blocker : blockers)
                    blocker();
                });
            blockers.clear();
        }

        // Whether an operation runs or starts with the next update
        bool IsPending() const { return block || !blockers.empty(); }

        // For callers which destroy what the operations reference before the next update
        void Finish() {
            JobSystem::Wait(job);
            for (const auto& blocker : blockers)
                blocker();
            blockers.clear();
            block = false;
        }

        JobGroup job { "Blocking operation" };
        std::vector<std::function<void()>> blockers;
        std::string blockText;
        bool block = false;

    };

}
