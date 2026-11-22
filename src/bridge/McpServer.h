#pragma once

#include "Common.h"

#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

namespace Atlas::Bridge {

    // Model Context Protocol server over Streamable HTTP on 127.0.0.1 (POST /mcp, answered with JSON). Every program
    // linking the bridge library can offer one: the editor, AtlasBridge.exe, a game. Only one gets the port, the first,
    // the others are told who has it. Clients see the program in serverInfo and its tools in tools/list. Requests
    // arrive on a thread of the server, tool calls run on the main thread of the program in Update() unless the tool
    // runs on the request thread
    class McpServer {

    public:
        using Reply = std::function<void(json result)>;

        struct Tool {
            std::string name;
            std::string description;
            json inputSchema = json::object({ { "type", "object" } });
            // Calls reply once with the result (TextContent, ImageContent), also in a later frame
            std::function<void(const json& arguments, const Reply& reply)> call;
            // Long calls which don't touch the program, e.g. starting a job, don't wait for its frames
            bool onRequestThread = false;
        };

        McpServer(const std::string& name, const std::string& instructions);

        ~McpServer();

        // Before Start
        void AddTool(Tool tool);

        // False if the port is taken, the error says by which program. With a token, requests need it as bearer token
        bool Start(int32_t port, std::string& error, const std::string& token = "");

        // Runs the calls which arrived since the last frame, on the main thread
        void Update();

        // Blocks while the server runs, for programs without frames
        void Wait();

        int32_t GetPort() const { return port; }

        static json TextContent(const std::string& text, bool isError = false);

        // Images wider than a sheet are sent at 1536 px
        static json ImageContent(fs::path path);

        static constexpr int32_t defaultPort = 7777;

    private:
        struct Call {
            const Tool* tool;
            json arguments;
            std::shared_ptr<std::promise<json>> result;
        };

        struct Http;

        std::optional<json> HandleMessage(const json& message);

        json CallTool(const std::string& name, const json& arguments);

        static void Run(const Call& call);

        std::string name;
        std::string instructions;
        std::vector<Tool> tools;

        std::unique_ptr<Http> http;
        std::thread thread;
        int32_t port = 0;

        std::mutex mutex;
        std::deque<Call> calls;
        bool closing = false;

    };

}
