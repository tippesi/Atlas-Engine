#pragma once

#include "Job.h"

namespace Atlas::Capture {

    // Finds mistakes in a job before its scene is loaded: Lua that doesn't compile, entity patterns that match no entity
    // of the scene file (with the closest names) and the preflight command of the project (capture.project.json) for
    // jobs that play. Issues start with ERROR (the job can't run) or NOTE (e.g. an entity a script may still create)
    std::vector<std::string> CheckJob(const Job& job, bool preflight = true);

    // Parses the job file with the options before it, a parse error is the only issue then
    std::vector<std::string> CheckJobFile(const fs::path& path, const std::vector<std::string>& options);

}
