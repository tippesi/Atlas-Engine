#include "McpTools.h"
#include "Processes.h"
#include "LuaEdit.h"

#include "project/Project.h"
#include "scripting/BindingRegistry.h"
#include "scripting/LuaHost.h"

#include <atomic>
#include <fstream>
#include <mutex>

namespace Atlas::Bridge {

    static std::atomic<int32_t> callCount = 0;

    static const char* runDescription = R"(Runs a bridge job with the real GPU on the bridge server (keeps the scene loaded between jobs) and returns
the summary: status, failed checks and expectations, results, outputs. A job is a JSON object or the path of a job
file: { "scene": "scenes/x.aescene", "queries": [], "edit": {}, "shots": [], "play": true, "duration": 5,
"timeline": [], "checks": {} }; docs("Jobs") has every key. Edits write scene files (only after their checks pass),
persistent scene changes go through "edit", never by rewriting .aescene files. Pass "--streaming" for large scenes.
Exit codes: 0 passed, 1 a check failed, 2 error, 3 crash, 4 still waiting (job_status, the job keeps running).)";

    std::string McpTools::GetLuaHelp() {

        return R"(The code sees the engine bindings (Atlas, Glm, scene, the globals of the scene scripts) and the table Bridge:
  Find(pattern), FindMeshes(pattern)    entities by name pattern (* ?) or #id, by loaded mesh path
  Query(query)                          one query of the job format, e.g. { type = "path", points = {...} };
                                        docs("Queries") lists them, rays and heights are scene:CastRay
  SampleTerrain(x, z)                   { height, slope, normal, material, hole } of the terrain, nil outside
  PickRay(camera, x, y)                 the ray through a pixel of a shot camera (jobs that render)
  Result(key, value), Fail(message)     returned with the result, a returned value comes back as "value"
  Time()                                seconds of the job or the call
  Selection(), Camera()                 selected entities and camera { location, direction } of the editor
  Touch(entity)                         call before changing components of an entity directly (undo)
  SetPosition(entity, Glm.Vec3(x,y,z))  world position, keeps the rotation; SetMatrix(entity, mat4) the local matrix
  Create(name, parent?), Destroy(entity)
  Edit(operation)                       one edit operation of the job format, e.g. { op = "snapToGround", entity = "Barrel" }
Scene settings are part of the step too, e.g. scene.sky.atmosphere = Atlas.Atmosphere.new(), scene.sky.probe = nil,
scene.sky.probe = Atlas.EnvironmentProbe.new("environment.hdr"), scene.sky.clouds = Atlas.VolumetricClouds.new().
Material graphs: Atlas.ResourceManager.GetOrLoadMaterialGraph(path):Get() or material.graph:Get(), then graph:FindNodes,
AddNode(type), Connect(from, pin, to, pin), SetInput/SetValue/SetName/SetCode(id, ...), Compile() (returns the errors),
Atlas.MaterialGraph.GetNodeTypes(). Atlas.Log.GetLatest(count) returns the newest log entries.)";

    }

    std::string McpTools::GetInstructions(const std::string& program) {

        return "This is the " + program + " of the Atlas Engine (serverInfo says which program, status what it has open). "
            R"(Measure before placing anything (scene:CastRay, Bridge.SampleTerrain, queries of jobs), never guess coordinates or
heights. Test time is the user's time: check a job first, run one targeted job with the shortest duration, look at
sheet.jpg (view_image) instead of single images. docs returns sections of the full reference.)";

    }

    json McpTools::ObjectSchema(const json& properties, const std::vector<std::string>& required) {

        return { { "type", "object" }, { "properties", properties }, { "required", required } };

    }

    json McpTools::RunBridge(const fs::path& bridgeExecutable, const fs::path& baseDirectory, std::vector<std::string> arguments,
        float timeoutSeconds) {

        auto directory = fs::temp_directory_path() / "AtlasBridge";
        fs::create_directories(directory);
        auto log = directory / ("mcp-" + std::to_string(CurrentProcessId()) + "-" + std::to_string(callCount++) + ".log");

        arguments.insert(arguments.begin(), bridgeExecutable.string());
        auto exitCode = RunProcess(arguments, baseDirectory, log, timeoutSeconds);

        std::ifstream stream(log, std::ios::binary);
        std::string output((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        output += "\n(exit code " + std::to_string(exitCode) + ")";
        return McpServer::TextContent(output, exitCode < 0 || (exitCode >= 2 && exitCode != 4));

    }

    void McpTools::AddJobTools(McpServer& server, const fs::path& bridgeExecutable, const fs::path& baseDirectory) {

        auto resolve = [baseDirectory](const std::string& path) {
            fs::path userPath(path);
            return userPath.is_absolute() ? userPath : (baseDirectory / userPath).lexically_normal();
            };
        auto writeJob = [resolve](const json& job) {
            if (job.is_string())
                return resolve(job.get<std::string>());

            auto directory = fs::temp_directory_path() / "AtlasBridge";
            fs::create_directories(directory);
            auto path = directory / ("mcp-job-" + std::to_string(CurrentProcessId()) + "-" + std::to_string(callCount++) + ".json");
            auto inlineJob = job;
            if (!inlineJob.contains("name"))
                inlineJob["name"] = "mcp";
            WriteFileAtomic(path, inlineJob.dump(2));
            return path;
            };
        auto run = [bridgeExecutable, baseDirectory](std::vector<std::string> arguments, float timeout = 0.0f) {
            return RunBridge(bridgeExecutable, baseDirectory, std::move(arguments), timeout);
            };
        json jobArgument = { { "description", "Job object or path of a job file" } };

        server.AddTool({ .name = "run", .description = runDescription,
            .inputSchema = ObjectSchema({ { "job", jobArgument },
                { "args", { { "type", "array" }, { "items", { { "type", "string" } } },
                    { "description", "Job options, e.g. --streaming, --play 3, --views front,low, --no-shots, --timeout 540" } } } }, { "job" }),
            .call = [run, writeJob](const json& arguments, const McpServer::Reply& reply) {
                std::vector<std::string> command = { "run", writeJob(arguments.at("job")).string() };
                for (const auto& arg : arguments.value("args", std::vector<std::string>()))
                    command.push_back(arg);
                reply(run(command));
                },
            .onRequestThread = true });

        server.AddTool({ .name = "check",
            .description = "Finds mistakes in a job without loading anything (entity names, Lua that doesn't compile, keys), milliseconds",
            .inputSchema = ObjectSchema({ { "job", jobArgument } }, { "job" }),
            .call = [run, writeJob](const json& arguments, const McpServer::Reply& reply) {
                reply(run({ "check", writeJob(arguments.at("job")).string() }, 120.0f));
                },
            .onRequestThread = true });

        server.AddTool({ .name = "inspect",
            .description = "Entities, meshes, materials and graph nodes of a scene, prefab, mesh, model or material graph file without loading it",
            .inputSchema = ObjectSchema({
                { "target", { { "type", "string" }, { "description", "File, or 'nodes' for the node types of material graphs" } } },
                { "filter", { { "type", "string" }, { "description", "Name pattern" } } },
                { "limit", { { "type", "integer" } } } }, { "target" }),
            .call = [run](const json& arguments, const McpServer::Reply& reply) {
                std::vector<std::string> command = { "inspect", arguments.at("target").get<std::string>() };
                if (arguments.contains("filter"))
                    command.insert(command.end(), { "--filter", arguments["filter"].get<std::string>() });
                if (arguments.contains("limit"))
                    command.insert(command.end(), { "--limit", std::to_string(arguments["limit"].get<int32_t>()) });
                reply(run(command, 300.0f));
                },
            .onRequestThread = true });

        server.AddTool({ .name = "job_status",
            .description = "What the bridge server and the engine applications on the machine do, whom a waiting run waits for",
            .call = [run](const json&, const McpServer::Reply& reply) { reply(run({ "status" }, 60.0f)); },
            .onRequestThread = true });

        server.AddTool({ .name = "cancel", .description = "Stops the running job of the bridge server",
            .call = [run](const json&, const McpServer::Reply& reply) { reply(run({ "cancel" }, 60.0f)); },
            .onRequestThread = true });

        server.AddTool({ .name = "view_image", .description = "An image file as an image, e.g. sheet.jpg of a job",
            .inputSchema = ObjectSchema({ { "path", { { "type", "string" } } } }, { "path" }),
            .call = [resolve](const json& arguments, const McpServer::Reply& reply) {
                reply(McpServer::ImageContent(resolve(arguments.at("path").get<std::string>())));
                },
            .onRequestThread = true });

        server.AddTool({ .name = "bindings",
            .description = "The Lua bindings: types with their fields, methods and constructors, the tables (Atlas, Glm, Host, "
                "Project, Bridge) and the globals of scripts, with Lua types. Search with filter (a type, table or member name, "
                "e.g. Sky, probe, SetPosition), or format lua for annotations of the Lua language server",
            .inputSchema = ObjectSchema({ { "filter", { { "type", "string" } } },
                { "format", { { "type", "string" }, { "enum", { "json", "lua" } } } } }),
            .call = [](const json& arguments, const McpServer::Reply& reply) {
                // A program only knows the bindings of the Lua states it created, all of them are created once here
                static std::once_flag recorded;
                std::call_once(recorded, []() {
                    Scripting::LuaHost host;
                    if (!Project::active)
                        Scripting::Binder(host.GetState().globals(), "_G").set("Project", CreateRef<Project>());
                    EditState state;
                    Scripting::Binder bridge(host.GetState().create_table(), "Bridge");
                    LuaEdit::AddFunctions(bridge, nullptr, state, {}, [](const std::string&, json) {}, []() { return 0.0f; });
                    });

                if (arguments.value("format", "json") == "lua") {
                    reply(McpServer::TextContent(Scripting::BindingRegistry::ToLuaAnnotations()));
                    return;
                }
                auto result = Scripting::BindingRegistry::ToJson(arguments.value("filter", ""));
                reply(McpServer::TextContent(result.empty() ? "Nothing matches the filter" : result.dump(1), result.empty()));
                },
            .onRequestThread = true });

        auto readme = bridgeExecutable.parent_path() / ".." / ".." / "src" / "bridge" / "README.md";
        server.AddTool({ .name = "docs",
            .description = "A section of the bridge reference (README.md) by its heading, e.g. Jobs, Playing, Checks, Queries, Editing, Pipelines; without a section the list of headings",
            .inputSchema = ObjectSchema({ { "section", { { "type", "string" } } } }),
            .call = [readme](const json& arguments, const McpServer::Reply& reply) {
                std::ifstream stream(readme);
                if (!stream.is_open()) {
                    reply(McpServer::TextContent("README.md of the bridge not found next to the build", true));
                    return;
                }

                std::vector<std::pair<std::string, std::string>> sections;
                std::string line;
                while (std::getline(stream, line)) {
                    if (!line.empty() && line.back() == '\r')
                        line.pop_back();
                    if (line.starts_with("#"))
                        sections.push_back({ line.substr(line.find_first_not_of("# ")), "" });
                    else if (!sections.empty())
                        sections.back().second += line + "\n";
                }

                auto section = arguments.value("section", "");
                std::string headings;
                for (const auto& [heading, text] : sections) {
                    if (!section.empty() && heading == section) {
                        reply(McpServer::TextContent("# " + heading + "\n" + text));
                        return;
                    }
                    headings += heading + "\n";
                }
                reply(McpServer::TextContent((section.empty() ? "" : "No section '" + section + "', the sections:\n") + headings,
                    !section.empty()));
                },
            .onRequestThread = true });

    }

}
