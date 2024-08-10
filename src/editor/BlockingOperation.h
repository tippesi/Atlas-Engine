#pragma once

#include <future>
#include <string>
#include <optional>

namespace Atlas::Editor {

    class BlockingOperation {

    public:
        void Block(const std::string& text, std::function<void()> blocker) {
            if (future.valid())
                future.get();
            this->blocker = blocker;
            blockText = text;
        }

        void Update() {
            if (!blocker.has_value())
                return;
            
            block = true;
            future = std::async(std::launch::async, [blocker = blocker, &block = block] {
                blocker.value()();
                block = false;
                });
            blocker = {};
        }

        std::future<void> future;
        std::optional<std::function<void()>> blocker;
        std::string blockText;
        bool block = false;

    };

}