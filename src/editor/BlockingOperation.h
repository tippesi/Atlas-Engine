#pragma once

#include "jobsystem/JobSystem.h"

#include <string>
#include <optional>

namespace Atlas::Editor {

    class BlockingOperation {

    public:
        ~BlockingOperation() { JobSystem::Wait(job); }

        void Block(const std::string& text, std::function<void()> blocker) {
            this->blocker = blocker;
            blockText = text;
        }

        void Update() {
            if (!blocker.has_value())
                return;
            
            block = true;
            JobSystem::Execute(job, [blocker = blocker, &block = block](JobData&) {
                blocker.value()();
                block = false;
                });
            blocker = {};
        }

        JobGroup job;
        std::optional<std::function<void()>> blocker;
        std::string blockText;
        bool block = false;

    };

}