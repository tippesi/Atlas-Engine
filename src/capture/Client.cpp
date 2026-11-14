#include "Client.h"
#include "Check.h"
#include "LuaJob.h"
#include "Process.h"
#include "Summary.h"

#include "InstanceRegistry.h"

#include <cstdio>
#include <fstream>
#include <thread>

namespace Atlas::Capture {

    struct ClientOptions {

        fs::path server;
        bool detach = false;
        float timeout = 540.0f;
        bool waitGpu = true;
        // Options of the server process itself, a server started with others is restarted
        std::vector<std::string> serverOptions;
        // The command and its arguments, job options are passed on to the job
        std::vector<std::string> arguments;

    };

    static void Sleep(int32_t milliseconds) {

        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));

    }

    static int64_t Now() {

        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    }

    static json ReadJson(const fs::path& path) {

        std::ifstream stream(path);
        if (!stream.is_open())
            return json();
        auto j = json::parse(stream, nullptr, false, true);
        return j.is_discarded() ? json() : j;

    }

    static fs::path GetDefaultServerDirectory() {

        auto executable = GetExecutablePath();
        return (executable.parent_path() / ".." / "capture-server" / executable.stem()).lexically_normal();

    }

    static std::string GetLogTail(const fs::path& path, size_t lines) {

        std::ifstream stream(path);
        std::vector<std::string> all;
        for (std::string line; std::getline(stream, line);)
            all.push_back(line);

        std::string tail;
        for (size_t i = all.size() > lines ? all.size() - lines : 0; i < all.size(); i++)
            tail += "\n    " + all[i];
        return tail;

    }

    // server.json of a server whose process still runs
    static std::optional<json> ReadServer(const fs::path& directory) {

        auto status = ReadJson(directory / "server.json");
        if (!status.is_object() || !IsProcessRunning(status.value("pid", uint32_t(0))))
            return std::nullopt;
        return status;

    }

    static bool IsCurrentServer(const json& status, const ClientOptions& options) {

        std::error_code error;
        auto executable = GetExecutablePath();
        auto build = fs::last_write_time(executable, error).time_since_epoch().count();
        return fs::path(status.value("executable", std::string())) == executable && status.value("build", int64_t(0)) == build &&
            status.value("options", std::vector<std::string>()) == options.serverOptions;

    }

    static std::string WriteCommand(const fs::path& directory, const json& command, const std::string& name) {

        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        auto file = std::to_string(milliseconds) + "_" + std::to_string(CurrentProcessId()) + "_" + SanitizeFileName(name);
        WriteFileAtomic(directory / (file + ".json"), command.dump(2));
        return file;

    }

    static bool WaitForExit(uint32_t processId, float timeoutSeconds) {

        auto start = std::chrono::steady_clock::now();
        while (IsProcessRunning(processId)) {
            if (SecondsSince(start) > timeoutSeconds)
                return false;
            Sleep(200);
        }
        return true;

    }

    static void StopServer(const fs::path& directory, const json& status) {

        auto processId = status.value("pid", uint32_t(0));
        if (status.value("state", std::string()) != "idle")
            WriteFileAtomic(directory / "cancel", "");
        WriteCommand(directory, { { "command", "stop" } }, "stop");
        if (!WaitForExit(processId, 60.0f)) {
            Print("The server didn't stop within 60 s, ending it");
            KillProcessTree(processId);
        }

    }

    // Starts a server or uses the running one, which has to be of this build and have the same options
    static std::optional<json> GetServer(const ClientOptions& options) {

        const auto& directory = options.server;
        auto status = ReadServer(directory);
        if (status && !IsCurrentServer(*status, options)) {
            auto start = std::chrono::steady_clock::now();
            if (status->value("state", std::string()) != "idle")
                Print("The server of " + directory.string() + " runs another build or other options and is busy with " +
                    status->value("job", std::string("a command")) + ", waiting for it");
            while (status && status->value("state", std::string()) != "idle" && SecondsSince(start) < options.timeout) {
                Sleep(500);
                status = ReadServer(directory);
            }
            if (status && status->value("state", std::string()) != "idle") {
                Print("The server is still busy, try again later or stop it with: AtlasEngineCapture stop");
                return std::nullopt;
            }
            if (status) {
                Print("Restarting the server, it runs another build or other options");
                StopServer(directory, *status);
            }
            status.reset();
        }
        if (status)
            return status;

        std::error_code error;
        fs::create_directories(directory, error);
        fs::remove(directory / "server.json", error);
        auto log = directory / "server.log";
        if (fs::exists(log, error))
            fs::rename(log, directory / "server.previous.log", error);

        std::vector<std::string> serverArgs = { "--serve", directory.string() };
        serverArgs.insert(serverArgs.end(), options.serverOptions.begin(), options.serverOptions.end());
        auto executable = GetExecutablePath();
        auto processId = StartDetachedProcess(executable, serverArgs, executable.parent_path(), log);
        if (processId == 0) {
            Print("Couldn't start a server with " + executable.string());
            return std::nullopt;
        }

        auto start = std::chrono::steady_clock::now();
        while (true) {
            status = ReadServer(directory);
            if (status && status->value("pid", uint32_t(0)) == processId)
                break;
            if (!IsProcessRunning(processId) || SecondsSince(start) > 120.0f) {
                Print("The server didn't start, its log:" + GetLogTail(log, 15));
                return std::nullopt;
            }
            Sleep(200);
        }
        Print("Started a server (process " + std::to_string(processId) + ") in " + directory.string());
        return status;

    }

    static int32_t PrintResult(const json& result, bool valuesOnly) {

        if (!result.contains("report")) {
            Print(result.value("command", std::string("command")) + ": " + result.value("status", std::string("error")) +
                (result.contains("reason") && !result["reason"].get<std::string>().empty() ? ": " + result["reason"].get<std::string>() : ""));
            return result.value("status", std::string()) == "ok" ? 0 : 2;
        }

        auto report = ReadJson(result["report"].get<std::string>());
        if (!report.is_object()) {
            Print("The report " + result["report"].get<std::string>() + " is missing: " + result.value("reason", std::string()));
            return 2;
        }

        auto exitCode = report.value("exitCode", 2);
        if (valuesOnly && exitCode == 0 && report.contains("results")) {
            for (const auto& [key, value] : report["results"].items())
                Print(key == "value" ? value.dump() : key + " = " + value.dump());
            return 0;
        }

        for (const auto& line : FormatSummary(report, fs::path(result.value("outputDirectory", std::string()))))
            Print(line);
        return exitCode;

    }

    static int32_t WaitForCommand(const ClientOptions& options, const std::string& command, bool valuesOnly) {

        auto path = options.server / (command + ".result.json");
        auto start = std::chrono::steady_clock::now();
        bool printedGpuWait = false;
        while (!fs::exists(path)) {
            auto status = ReadServer(options.server);
            if (!status && !fs::exists(path)) {
                Print("The server ended before " + command + " was done, its log:" + GetLogTail(options.server / "server.log", 20));
                return 3;
            }
            if (status && !printedGpuWait && status->value("state", std::string()) == "waiting for the GPU" &&
                status->value("command", std::string()) == command) {
                Print("The server waits for the GPU before it loads " + status->value("scene", std::string()) + ", status shows for whom");
                printedGpuWait = true;
            }
            if (options.timeout > 0.0f && SecondsSince(start) > options.timeout) {
                Print(command + " still runs after " + FormatFloat(options.timeout, 0) + " s, wait for it with: AtlasEngineCapture wait " +
                    command + (options.server == GetDefaultServerDirectory() ? "" : " --server " + options.server.string()));
                return 4;
            }
            Sleep(200);
        }

        // The result is written atomically
        return PrintResult(ReadJson(path), valuesOnly);

    }

    static int32_t SendCommand(const ClientOptions& options, const json& command, const std::string& name, bool valuesOnly = false) {

        auto server = GetServer(options);
        if (!server)
            return 2;

        // The server waits for the GPU itself before it loads a scene
        auto serverProcess = server->value("pid", uint32_t(0));
        if (!options.waitGpu) {
            for (const auto& instance : InstanceRegistry::GetInstances())
                if (instance.busy && instance.pid != serverProcess && instance.pid != CurrentProcessId())
                    Print("Note: " + instance.executable + " (process " + std::to_string(instance.pid) + ") is busy: " + instance.activity);
        }

        auto file = WriteCommand(options.server, command, name);
        if (options.detach) {
            Print("Sent " + file + ", wait for it with: AtlasEngineCapture wait " + file);
            return 0;
        }

        return WaitForCommand(options, file, valuesOnly);

    }

    static int32_t Run(ClientOptions& options) {

        if (options.arguments.size() < 2)
            throw std::runtime_error("run expects a job file or an inline job");

        const auto& jobArgument = options.arguments[1];
        std::vector<std::string> jobOptions(options.arguments.begin() + 2, options.arguments.end());
        if (!options.waitGpu)
            jobOptions.push_back("--no-wait-gpu");

        json command = { { "command", "run" }, { "args", jobOptions } };
        std::string name;
        if (jobArgument.starts_with("{")) {
            auto inlineJob = json::parse(jobArgument, nullptr, false, true);
            if (!inlineJob.is_object())
                throw std::runtime_error("The inline job isn't a JSON object");
            command["job"] = inlineJob;
            name = inlineJob.value("name", std::string("inline"));
        }
        else {
            auto path = ResolveUserPath(jobArgument);
            // Mistakes in the job are found without the server
            auto issues = CheckJobFile(path, jobOptions);
            if (!issues.empty() && issues.front().starts_with("ERROR")) {
                for (const auto& issue : issues)
                    Print(issue);
                return 2;
            }
            for (const auto& issue : issues)
                Print(issue);
            command["job"] = path.generic_string();
            name = path.stem().string();
        }

        return SendCommand(options, command, name);

    }

    static int32_t Eval(ClientOptions& options) {

        if (options.arguments.size() < 2)
            throw std::runtime_error("eval expects Lua code");

        float play = 0.05f;
        std::string scene;
        auto& arguments = options.arguments;
        for (size_t i = 2; i < arguments.size(); i++) {
            if (arguments[i] == "--play" && i + 1 < arguments.size())
                play = std::max(std::stof(arguments[++i]), 0.05f);
            else if (arguments[i] == "--scene" && i + 1 < arguments.size())
                scene = arguments[++i];
            else
                throw std::runtime_error("Unknown eval option '" + arguments[i] + "' (--play <seconds>, --scene <scene>)");
        }

        auto error = LuaJob::Compile(arguments[1], "eval", true);
        if (!error.empty()) {
            Print("ERROR: " + error);
            return 2;
        }

        json job = {
            { "name", "eval" }, { "play", true }, { "duration", play }, { "shots", json::array() }, { "realTime", false },
            { "timeline", json::array({ { { "t", play }, { "lua", arguments[1] }, { "result", "value" } } }) }
        };
        if (!scene.empty())
            job["scene"] = scene;
        if (!options.waitGpu)
            job["waitGpu"] = false;

        return SendCommand(options, { { "command", "run" }, { "job", job } }, "eval", true);

    }

    static int32_t Status(const ClientOptions& options) {

        auto status = ReadServer(options.server);
        if (!status) {
            Print("No server runs in " + options.server.string());
        }
        else {
            auto since = Now() - status->value("since", Now());
            auto state = status->value("state", std::string());
            Print("Server (process " + std::to_string(status->value("pid", 0)) + ") in " + options.server.string() + ": " + state +
                (state == "running" ? " " + status->value("job", std::string()) : "") + " for " + std::to_string(since) + " s" +
                (status->value("scene", std::string()).empty() ? "" : ", scene " + status->value("scene", std::string())) +
                (IsCurrentServer(*status, options) ? "" : ", another build"));
        }
        PrintInstances();
        return 0;

    }

    std::optional<int32_t> RunClient(const std::vector<std::string>& args) {

        static const std::vector<std::string> commands = { "run", "eval", "wait", "status", "cancel", "stop" };
        if (args.size() < 2 || std::find(commands.begin(), commands.end(), args[1]) == commands.end())
            return std::nullopt;

        ClientOptions options;
        options.server = GetDefaultServerDirectory();
        for (size_t i = 1; i < args.size(); i++) {
            const auto& arg = args[i];
            if (arg == "--server" && i + 1 < args.size())
                options.server = ResolveUserPath(args[++i]);
            else if (arg == "--detach")
                options.detach = true;
            else if (arg == "--timeout" && i + 1 < args.size())
                options.timeout = std::stof(args[++i]);
            else if (arg == "--no-wait-gpu")
                options.waitGpu = false;
            else if (arg == "--wait-gpu") {
                // The default, accepted for older command lines
            }
            else if (arg == "--streaming" || arg == "--validation")
                options.serverOptions.push_back(arg);
            else
                options.arguments.push_back(arg);
        }

        const auto& command = options.arguments.front();
        try {
            if (command == "run")
                return Run(options);
            if (command == "eval")
                return Eval(options);
            if (command == "status")
                return Status(options);
            if (command == "wait") {
                if (options.arguments.size() < 2)
                    throw std::runtime_error("wait expects the name of a command");
                return WaitForCommand(options, options.arguments[1], false);
            }

            auto status = ReadServer(options.server);
            if (!status) {
                Print("No server runs in " + options.server.string());
                return 0;
            }
            if (command == "cancel") {
                WriteFileAtomic(options.server / "cancel", "");
                Print("Asked the server to cancel " + status->value("job", std::string("its job")));
                return 0;
            }
            StopServer(options.server, *status);
            Print("Stopped the server");
            return 0;
        }
        catch (const std::exception& e) {
            Print(std::string("ERROR: ") + e.what());
            return 2;
        }

    }

    void PrintInstances() {

        auto all = InstanceRegistry::GetInstances();
        auto instances = all;
        std::erase_if(instances, [](const auto& instance) { return instance.pid == CurrentProcessId(); });
        if (instances.empty()) {
            Print("No other engine application runs");
            return;
        }

        auto now = Now();
        Print("Engine applications:");
        for (const auto& instance : instances) {
            std::string arguments;
            for (const auto& arg : instance.args)
                arguments += " " + arg;
            if (arguments.size() > 100)
                arguments = arguments.substr(0, 97) + "...";
            std::string waitsFor;
            if (instance.waitingSince)
                for (const auto& other : InstanceRegistry::GetInstancesAhead(instance, all))
                    waitsFor += (waitsFor.empty() ? ", waits for " : ", ") + std::to_string(other.pid);
            Print("  " + std::to_string(instance.pid) + " " + instance.executable + (instance.busy ? " [busy] " : " ") + instance.activity +
                waitsFor + ", running " + std::to_string((now - instance.started) / 60) + " min" +
                (instance.expectedEnd ? ", expected end in " + std::to_string(std::max(*instance.expectedEnd - now, int64_t(0))) + " s" : "") +
                " |" + arguments);
        }

    }

    bool WaitForGpu(float timeoutSeconds) {

        auto start = std::chrono::steady_clock::now();
        bool printed = false;
        while (true) {
            auto busy = InstanceRegistry::GetBusyInstances();
            if (busy.empty())
                return true;

            if (!printed) {
                for (const auto& instance : busy)
                    Print("Waiting for " + instance.executable + " (process " + std::to_string(instance.pid) + "): " + instance.activity);
                printed = true;
            }
            if (timeoutSeconds > 0.0f && SecondsSince(start) > timeoutSeconds) {
                Print("The GPU is still busy after " + FormatFloat(timeoutSeconds, 0) + " s");
                return false;
            }
            Sleep(1000);
        }

    }

    int32_t Kill(const std::string& target) {

        std::vector<uint32_t> processes;
        if (target == "captures") {
            for (const auto& instance : InstanceRegistry::GetInstances())
                if (instance.pid != CurrentProcessId() && instance.executable.starts_with("AtlasEngineCapture"))
                    processes.push_back(instance.pid);
        }
        else {
            processes.push_back(uint32_t(std::stoul(target)));
        }

        for (auto processId : processes) {
            auto killed = KillProcessTree(processId);
            Print("Ended process " + std::to_string(processId) + (killed.size() > 1 ? " and " + std::to_string(killed.size() - 1) +
                " processes it started" : killed.empty() ? " (it didn't run)" : ""));
        }
        if (processes.empty())
            Print("No other capture tool runs");
        return 0;

    }

}
