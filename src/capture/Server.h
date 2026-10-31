#pragma once

#include "Common.h"

#include <chrono>
#include <optional>

namespace Atlas::Capture {

    // The command queue of the server mode: JSON files in a directory, processed in the order of their file names.
    // A processed command is moved to done/, its result is written to <command>.result.json. See README.md
    class Server {

    public:
        explicit Server(const fs::path& directory);

        // The next command, the directory is read at most every 200 ms. Files with invalid JSON are returned as
        // a discarded value
        std::optional<json> Poll();

        // Writes the result of the current command, with its duration and the loaded scene
        void Complete(const json& result, const std::string& scenePath);

        fs::path directory;
        // File name of the current command without its extension
        std::string commandName;

    private:
        std::chrono::steady_clock::time_point commandStartTime;
        std::chrono::steady_clock::time_point nextPollTime;

    };

}
