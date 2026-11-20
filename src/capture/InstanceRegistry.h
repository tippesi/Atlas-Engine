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
            // Milliseconds since the epoch, while it waits for the GPU. A waiting instance which is busy claimed the GPU
            std::optional<int64_t> waitingSince;

        };

        static void Register(const std::vector<std::string>& args);

        static void Unregister();

        // Ends waiting for the GPU
        static void SetActivity(const std::string& activity, bool busy, std::optional<int64_t> expectedEnd = std::nullopt);

        // The instances whose process still runs, files of ended processes are removed
        static std::vector<Instance> GetInstances();

        // The other instances which keep the GPU busy
        static std::vector<Instance> GetBusyInstances();

        // The instances a waiting instance waits for: busy ones and the ones which wait longer, the smaller process id
        // first on a tie. Without waiting it's the busy ones
        static std::vector<Instance> GetInstancesAhead(const Instance& waiter, const std::vector<Instance>& instances);

        // Queues this process for the GPU, it isn't busy while it waits
        static void QueueForGpu(const std::string& activity);

        // One step of waiting, at most every few hundred milliseconds: with nobody ahead this process claims the GPU and
        // gets it when the next step still finds nobody ahead, two processes claiming at once see each other then.
        // Returns true once the caller may load, ahead lists whom it still waits for
        static bool PollGpu(std::vector<Instance>& ahead);

        // E.g. "AtlasEngineCapture.exe (process 1234): job hours, ..."
        static std::string Describe(const std::vector<Instance>& instances);

    };

}
