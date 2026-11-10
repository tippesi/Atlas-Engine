#pragma once

#include "Common.h"

#include <optional>

namespace Atlas::Capture {

    // The client of the server mode: starts a server or uses the running one, keeps its scene loaded between runs,
    // sends a command, waits for it and prints the summary of the job.
    //   run <job.json | inline job> [job options]   eval <lua> [--play <s>] [--scene <scene>]
    //   wait <command>   status   cancel   stop
    // Common options: --server <dir> (default bin/capture-server/<executable>), --detach (return right away),
    // --timeout <s> (stop waiting, default 540), --wait-gpu (wait until no other engine application is busy)
    std::optional<int32_t> RunClient(const std::vector<std::string>& args);

    // Prints the engine applications that run on this machine and what they do, see InstanceRegistry
    void PrintInstances();

    // Waits until no other engine application keeps the GPU busy. Returns false after the timeout
    bool WaitForGpu(float timeoutSeconds, uint32_t ignoredProcess = 0);

    // Ends a process with its children, "captures" ends all other capture tools
    int32_t Kill(const std::string& target);

}
