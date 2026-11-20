#pragma once

#include "Common.h"

#include <chrono>
#include <optional>

namespace Atlas::Capture {

    // The command queue of the server mode: JSON files in a directory, processed in the order of their file names.
    // A processed command is moved to done/, its result is written to <command>.result.json. server.json tells
    // clients which process serves the directory, its build and what it does; a file named cancel stops the running
    // job. The client mode of the executable writes the commands (Client.h)
    class Server {

    public:
        // The options are the server wide options of the command line, e.g. --streaming
        Server(const fs::path& directory, const std::vector<std::string>& options);

        // The next command, the directory is read at most every 200 ms. Files with invalid JSON are returned as
        // a discarded value
        std::optional<json> Poll();

        // Writes the result of the current command, with its duration and the loaded scene
        void Complete(const json& result, const std::string& scenePath);

        // Writes server.json: idle, loading or running, the loaded scene and the job
        void WriteStatus(const std::string& state, const std::string& scenePath, const std::string& job = "");

        // Whether a client asked to stop the running job, checked at most every 200 ms
        bool IsCancelled();

        // Removes server.json when the server ends
        void Close();

        fs::path directory;
        // File name of the current command without its extension
        std::string commandName;

    private:
        std::vector<std::string> options;
        // Write time of the executable when the server started, a client restarts a server of an older build
        int64_t build = 0;

        std::chrono::steady_clock::time_point commandStartTime;
        std::chrono::steady_clock::time_point nextPollTime;
        std::chrono::steady_clock::time_point nextCancelCheck;

    };

}
