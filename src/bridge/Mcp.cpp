#include "Mcp.h"
#include "McpTools.h"
#include "Processes.h"

namespace Atlas::Bridge {

    int32_t RunMcpServer(const std::vector<std::string>& args) {

        auto port = McpServer::defaultPort;
        std::string token;
        if (auto environment = std::getenv("ATLAS_BRIDGE_TOKEN"))
            token = environment;

        for (size_t i = 2; i < args.size(); i++) {
            auto hasValue = i + 1 < args.size();
            if (args[i] == "--port" && hasValue)
                port = std::stoi(args[++i]);
            else if (args[i] == "--token" && hasValue)
                token = args[++i];
            else
                throw std::runtime_error("Usage: mcp [--port 7777] [--token <token>]");
        }

        auto executable = GetExecutablePath();
        auto base = GetLaunchDirectory();

        McpServer server("atlas-bridge", McpTools::GetInstructions("bridge, which runs jobs on scene files with the real GPU"));
        McpTools::AddJobTools(server, executable, base);

        server.AddTool({ .name = "lua",
            .description = "Runs Lua in the scene the bridge server has loaded, after playing it for play seconds. " + McpTools::GetLuaHelp(),
            .inputSchema = McpTools::ObjectSchema({
                { "code", { { "type", "string" } } },
                { "scene", { { "type", "string" }, { "description", "Scene to load, default the loaded one" } } },
                { "play", { { "type", "number" } } } }, { "code" }),
            .call = [executable, base](const json& arguments, const McpServer::Reply& reply) {
                std::vector<std::string> command = { "eval", arguments.at("code").get<std::string>() };
                if (arguments.contains("scene"))
                    command.insert(command.end(), { "--scene", arguments["scene"].get<std::string>() });
                if (arguments.contains("play"))
                    command.insert(command.end(), { "--play", std::to_string(arguments["play"].get<float>()) });
                reply(McpTools::RunBridge(executable, base, command));
                },
            .onRequestThread = true });

        server.AddTool({ .name = "status", .description = "Which program serves the tools (the bridge) and what its server does",
            .call = [executable, base](const json&, const McpServer::Reply& reply) {
                reply(McpTools::RunBridge(executable, base, { "status" }, 60.0f));
                },
            .onRequestThread = true });

        std::string error;
        if (!server.Start(port, error, token)) {
            Print("ERROR: " + error);
            return 2;
        }
        Print("Atlas Bridge MCP server on http://127.0.0.1:" + std::to_string(port) + "/mcp");
        server.Wait();
        return 0;

    }

}
