#pragma once

#include <future>
#include <string>

namespace Atlas::Editor {

    class BlockingOperation {

    public:
        void Block(const std::string& text, std::function<void()> blocker) {
            if (future.valid())
                future.get();
            block = true;
            blockText = text;
            future = std::async(std::launch::async, [blocker, &block = block] {
                blocker();
                block = false;
                });
        }

        std::future<void> future;
        std::string blockText;
        bool block = false;

    };

}