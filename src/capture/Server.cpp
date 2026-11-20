#include "Server.h"
#include "Process.h"

#include <algorithm>
#include <fstream>

namespace Atlas::Capture {

    Server::Server(const fs::path& directory, const std::vector<std::string>& options) : directory(directory), options(options) {

        fs::create_directories(directory / "done");
        std::error_code error;
        fs::remove(directory / "cancel", error);
        build = fs::last_write_time(GetExecutablePath(), error).time_since_epoch().count();

    }

    std::optional<json> Server::Poll() {

        auto now = std::chrono::steady_clock::now();
        if (now < nextPollTime)
            return std::nullopt;
        nextPollTime = now + std::chrono::milliseconds(200);

        std::vector<fs::path> files;
        std::error_code error;
        for (const auto& entry : fs::directory_iterator(directory, error)) {
            auto name = entry.path().filename().string();
            if (entry.is_regular_file() && name.ends_with(".json") && !name.ends_with(".result.json") && name != "server.json")
                files.push_back(entry.path());
        }
        if (files.empty())
            return std::nullopt;
        std::sort(files.begin(), files.end());

        auto file = files.front();
        commandName = file.stem().string();
        commandStartTime = now;

        json command;
        {
            std::ifstream stream(file);
            command = json::parse(stream, nullptr, false, true);
        }
        // Processed commands are moved away, their results are written next to the queue
        auto processed = directory / "done" / file.filename();
        fs::rename(file, processed, error);
        if (error) {
            fs::copy_file(file, processed, fs::copy_options::overwrite_existing, error);
            fs::remove(file, error);
        }

        return command;

    }

    void Server::Complete(const json& result, const std::string& scenePath) {

        auto complete = result;
        complete["command"] = commandName;
        complete["seconds"] = std::chrono::duration<float>(std::chrono::steady_clock::now() - commandStartTime).count();
        complete["scene"] = scenePath.empty() ? json() : json(scenePath);
        WriteFileAtomic(directory / (commandName + ".result.json"), complete.dump(2));

    }

    void Server::WriteStatus(const std::string& state, const std::string& scenePath, const std::string& job) {

        json status = {
            { "pid", CurrentProcessId() }, { "executable", GetExecutablePath().string() }, { "build", build }, { "options", options },
            { "state", state },
            { "scene", scenePath }, { "command", state == "idle" ? "" : commandName }, { "job", job },
            { "since", std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() }
        };
        WriteFileAtomic(directory / "server.json", status.dump(2));

    }

    bool Server::IsCancelled() {

        auto now = std::chrono::steady_clock::now();
        if (now < nextCancelCheck)
            return false;
        nextCancelCheck = now + std::chrono::milliseconds(200);

        std::error_code error;
        if (!fs::exists(directory / "cancel", error))
            return false;
        fs::remove(directory / "cancel", error);
        return true;

    }

    void Server::Close() {

        std::error_code error;
        fs::remove(directory / "server.json", error);

    }

}
