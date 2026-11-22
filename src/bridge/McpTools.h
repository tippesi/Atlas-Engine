#pragma once

#include "McpServer.h"

namespace Atlas::Bridge {

    // Tools of the MCP server which every program can offer: jobs on scene files run by AtlasBridge.exe (run, check,
    // inspect, job_status, cancel), images and the reference. The descriptions carry the rules of README.md
    class McpTools {

    public:
        // Relative paths of jobs and images are relative to the base directory, e.g. the project
        static void AddJobTools(McpServer& server, const fs::path& bridgeExecutable, const fs::path& baseDirectory);

        // The table Bridge of LuaEdit.h, for the descriptions of lua tools
        static std::string GetLuaHelp();

        static std::string GetInstructions(const std::string& program);

        static json ObjectSchema(const json& properties, const std::vector<std::string>& required = {});

        // Runs a command of AtlasBridge.exe and returns its output
        static json RunBridge(const fs::path& bridgeExecutable, const fs::path& baseDirectory, std::vector<std::string> arguments,
            float timeoutSeconds = 0.0f);

    };

}
