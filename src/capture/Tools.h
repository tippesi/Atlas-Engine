#pragma once

#include "Common.h"

#include <optional>

namespace Atlas::Capture {

    // The modes of the executable that need neither a window nor the GPU, they run before the engine starts: the client of
    // a server, check, kill, compose, diff, inspect and bench. A direct run of a job is checked here before the engine
    // starts, returns no exit code for every mode that renders
    std::optional<int32_t> RunCommandLineTool(const std::vector<std::string>& args, const std::string& assetDirectory);

}
