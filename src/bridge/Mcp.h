#pragma once

#include "Common.h"

namespace Atlas::Bridge {

    // `AtlasBridge mcp`: the MCP server of the bridge (McpServer.h) with the job tools and Lua in the scene of the bridge
    // server, until the process ends
    int32_t RunMcpServer(const std::vector<std::string>& args);

}
