#pragma once

#include "Common.h"

struct SDL_Window;

namespace Atlas::Capture {

    uint32_t CurrentProcessId();

    bool IsProcessRunning(uint32_t processId);

    fs::path GetExecutablePath();

    // Starts a process that keeps running after this one ended, its output goes into the log file. Returns its id, 0 if
    // it couldn't be started
    uint32_t StartDetachedProcess(const fs::path& executable, const std::vector<std::string>& args, const fs::path& workingDirectory,
        const fs::path& log);

    // Runs a command line (the program first) and waits for it, its output is written into the log file. Returns the exit
    // code, -1 if it couldn't be started and -2 if it was ended after the timeout
    int32_t RunProcess(const std::vector<std::string>& command, const fs::path& workingDirectory, const fs::path& log,
        float timeoutSeconds = 0.0f);

    // Ends a process and every process it started, returns the ids of the ended processes
    std::vector<uint32_t> KillProcessTree(uint32_t processId);

    // Puts a window behind all other windows without activating it, only on Windows
    void MoveWindowToBack(SDL_Window* window);

}
