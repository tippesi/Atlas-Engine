#pragma once

#include "Common.h"

namespace Atlas::Capture {

    // The result of a job in a few lines, from its report.json: status, failed checks and expectations grouped with
    // counts, conditions that never happened, results, errors and outputs. Printed at the end of a job and by the client
    std::vector<std::string> FormatSummary(const json& report, const fs::path& outputDirectory);

}
