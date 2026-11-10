#pragma once

#include "System.h"

#include <optional>
#include <string>
#include <vector>

namespace Atlas::Capture {

    // The engine applications running on this machine (capture tool, games linking the capture library), so that tools
    // see who uses the GPU. Each one is a file in a directory of the user, an application adds it in LoadContent and
    // removes it in UnloadContent
    class InstanceRegistry {

    public:
        struct Instance {

            uint32_t pid = 0;
            std::string executable;
            std::vector<std::string> args;
            // Seconds since the epoch
            int64_t started = 0;
            // What the application does, e.g. "job hours", and whether it keeps the GPU under load
            std::string activity;
            bool busy = true;
            std::optional<int64_t> expectedEnd;

        };

        static void Register(const std::vector<std::string>& args);

        static void Unregister();

        static void SetActivity(const std::string& activity, bool busy, std::optional<int64_t> expectedEnd = std::nullopt);

        // The instances whose process still runs, files of ended processes are removed
        static std::vector<Instance> GetInstances();

    };

}
