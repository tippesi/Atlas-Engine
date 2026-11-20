#pragma once

#include "Common.h"

namespace Atlas::Capture {

    // bench <bench.json>: runs jobs with two or more builds (executables) or shader directories in interleaved rounds with
    // the deterministic settings of the performance rules (1920x1080, 240 frames, the last 64 profiled, auto exposure off,
    // the scene paused right away) and reports the frame total and the passes as mean and spread per build. Rounds are added while a
    // difference is smaller than the spread. The images of two builds are compared with the noise of one build
    //   { "jobs": ["sponza.json"], "builds": { "base": "bin/RelWithDebInfo/AtlasEngineCapture_base.exe", "change": "..." },
    //     "rounds": 2, "maxRounds": 5, "passes": ["Render scene"] }
    // or "shaders": { "base": "<directory>", "change": "shader" } instead of builds, run with this executable
    int32_t RunBench(const std::vector<std::string>& args);

}
