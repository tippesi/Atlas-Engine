// Winsock has to come before any windows.h, so this file is compiled without the precompiled header, and windows.h
// mustn't leave its macros to the engine headers
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <httplib.h>
#undef near
#undef far

#include "McpServer.h"
#include "Image.h"

#include "Engine.h"

#include <fstream>
#include <random>

namespace Atlas::Bridge {

    struct McpServer::Http {
        httplib::Server server;
    };

    McpServer::McpServer(const std::string& name, const std::string& instructions)
        : name(name), instructions(instructions), http(std::make_unique<Http>()) {

    }

    McpServer::~McpServer() {

        // Calls still waiting get an answer first, stopping waits for their requests. Later calls are answered right away
        std::deque<Call> pending;
        {
            std::scoped_lock lock(mutex);
            closing = true;
            pending = std::exchange(calls, {});
        }
        for (auto& call : pending)
            call.result->set_value(TextContent("The " + name + " was closed", true));

        http->server.stop();
        if (thread.joinable())
            thread.join();

    }

    void McpServer::AddTool(Tool tool) {

        tools.push_back(std::move(tool));

    }

    bool McpServer::Start(int32_t port, std::string& error, const std::string& token) {

        auto& server = http->server;
        // The first program keeps the port, SO_REUSEADDR would let a second one bind it on Windows
        server.set_socket_options([](socket_t socket) {
            int enable = 1;
#ifdef _WIN32
            setsockopt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&enable), sizeof(enable));
#else
            setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
#endif
            });
        // Jobs run for minutes, the answer is written when they end
        server.set_read_timeout(60, 0);
        server.set_write_timeout(1800, 0);

        // Web pages must not reach the server: their requests carry an Origin, and with DNS rebinding a host name of the
        // attacker. A JSON content type can't be sent without a preflight, which isn't answered. Clients of agents send
        // no Origin, the host only matters without a token (a tunnel forwards other host names)
        auto authorized = [this, token](const httplib::Request& request, httplib::Response& response) {
            auto origin = request.get_header_value("Origin");
            auto knownOrigin = origin == "https://chatgpt.com" || origin == "https://claude.ai";
            auto hostAllowed = !token.empty() || IsLocalHost(request.get_header_value("Host"));
            if ((!IsLocalOrigin(origin) && !knownOrigin) || !hostAllowed) {
                response.status = 403;
                return false;
            }
            if (!token.empty() && request.get_header_value("Authorization") != "Bearer " + token) {
                response.status = 401;
                response.set_header("WWW-Authenticate", "Bearer");
                return false;
            }
            return true;
            };

        server.Post("/mcp", [this, authorized](const httplib::Request& request, httplib::Response& response) {
            if (!authorized(request, response))
                return;
            if (!request.get_header_value("Content-Type").starts_with("application/json")) {
                response.status = 415;
                return;
            }

            auto message = json::parse(request.body, nullptr, false);
            if (message.is_discarded()) {
                response.status = 400;
                response.set_content(json({ { "jsonrpc", "2.0" }, { "id", nullptr },
                    { "error", { { "code", -32700 }, { "message", "Parse error" } } } }).dump(), "application/json");
                return;
            }

            auto answer = HandleMessage(message);
            if (!answer) {
                response.status = 202;
                return;
            }
            // Every request stands on its own, the session id is only for clients which expect one
            if (message.value("method", "") == "initialize")
                response.set_header("Mcp-Session-Id", std::to_string(std::random_device()()) + std::to_string(std::random_device()()));
            response.set_content(answer->dump(), "application/json");
            });
        // No stream of messages from the server
        server.Get("/mcp", [authorized](const httplib::Request& request, httplib::Response& response) {
            if (authorized(request, response))
                response.status = 405;
            });
        server.Delete("/mcp", [authorized](const httplib::Request& request, httplib::Response& response) {
            if (authorized(request, response))
                response.status = 200;
            });

        if (!server.bind_to_port("127.0.0.1", port)) {
            // The program which has the port says its name when asked like a client would
            httplib::Client client("127.0.0.1", port);
            client.set_connection_timeout(1, 0);
            client.set_read_timeout(2, 0);
            auto other = client.Post("/mcp", json({ { "jsonrpc", "2.0" }, { "id", 1 }, { "method", "initialize" },
                { "params", json::object() } }).dump(), "application/json");
            auto answer = other && other->status == 200 ? json::parse(other->body, nullptr, false) : json();
            auto owner = answer.is_object() ? answer["result"]["serverInfo"].value("name", "") : "";
            error = "Port " + std::to_string(port) + (owner.empty() ? " is used by another program" : " is used by " + owner);
            return false;
        }

        this->port = port;
        thread = std::thread([this]() { http->server.listen_after_bind(); });
        return true;

    }

    void McpServer::Update() {

        std::deque<Call> pending;
        {
            std::scoped_lock lock(mutex);
            pending = std::exchange(calls, {});
        }
        for (const auto& call : pending)
            Run(call);

    }

    void McpServer::Wait() {

        if (thread.joinable())
            thread.join();

    }

    json McpServer::TextContent(const std::string& text, bool isError) {

        return { { "content", json::array({ { { "type", "text" }, { "text", text } } }) }, { "isError", isError } };

    }

    json McpServer::ImageContent(fs::path path) {

        // Large windows would be megabytes of base64 in the answer
        if (auto image = LoadImageFile(path); image && image->width > 1536) {
            auto preview = path;
            preview.replace_extension(".preview.jpg");
            if (WriteImage(Downscale(image, 1536), preview))
                path = preview;
        }

        std::ifstream stream(path, std::ios::binary);
        if (!stream.is_open())
            return TextContent("Can't read " + path.string(), true);
        std::string data((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

        auto extension = path.extension().string();
        auto mime = extension == ".jpg" || extension == ".jpeg" ? "image/jpeg" : "image/png";
        return { { "content", json::array({
            { { "type", "image" }, { "data", httplib::detail::base64_encode(data) }, { "mimeType", mime } },
            { { "type", "text" }, { "text", path.string() } } }) }, { "isError", false } };

    }

    std::optional<json> McpServer::HandleMessage(const json& message) {

        if (!message.is_object())
            return json { { "jsonrpc", "2.0" }, { "id", nullptr }, { "error", { { "code", -32600 }, { "message", "Invalid request" } } } };
        // Notifications and answers of the client get no answer
        if (!message.contains("id") || !message.contains("method"))
            return std::nullopt;

        auto method = message.value("method", "");
        auto params = message.value("params", json::object());
        json response = { { "jsonrpc", "2.0" }, { "id", message["id"] } };

        if (method == "initialize") {
            response["result"] = {
                { "protocolVersion", params.value("protocolVersion", "2025-06-18") },
                { "capabilities", { { "tools", json::object() } } },
                { "serverInfo", { { "name", name }, { "version", Engine::GetVersion() } } },
                { "instructions", instructions }
            };
        }
        else if (method == "tools/list") {
            json list = json::array();
            for (const auto& tool : tools)
                list.push_back({ { "name", tool.name }, { "description", tool.description }, { "inputSchema", tool.inputSchema } });
            response["result"] = { { "tools", list } };
        }
        else if (method == "tools/call") {
            response["result"] = CallTool(params.value("name", ""), params.value("arguments", json::object()));
        }
        else if (method == "ping") {
            response["result"] = json::object();
        }
        else {
            response["error"] = { { "code", -32601 }, { "message", "Method not found: " + method } };
        }

        return response;

    }

    json McpServer::CallTool(const std::string& toolName, const json& arguments) {

        auto tool = std::find_if(tools.begin(), tools.end(), [&](const Tool& tool) { return tool.name == toolName; });
        if (tool == tools.end())
            return TextContent("Unknown tool " + toolName + " of the " + name, true);

        Call call = { &*tool, arguments, std::make_shared<std::promise<json>>() };
        auto result = call.result->get_future();
        if (tool->onRequestThread) {
            Run(call);
        }
        else {
            std::scoped_lock lock(mutex);
            if (closing)
                return TextContent("The " + name + " was closed", true);
            calls.push_back(call);
        }

        // The program can be busy, e.g. loading a scene or starting to play
        if (result.wait_for(std::chrono::minutes(10)) != std::future_status::ready)
            return TextContent("The " + name + " didn't run the call within 10 minutes", true);
        return result.get();

    }

    void McpServer::Run(const Call& call) {

        auto replied = std::make_shared<bool>(false);
        auto reply = [call, replied](json result) {
            if (!std::exchange(*replied, true))
                call.result->set_value(std::move(result));
            };
        try {
            call.tool->call(call.arguments, reply);
        }
        catch (const std::exception& e) {
            reply(TextContent(std::string("ERROR: ") + e.what(), true));
        }

    }

}
