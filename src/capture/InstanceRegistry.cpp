#include "InstanceRegistry.h"
#include "Process.h"
#include "common/SerializationHelper.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

#ifdef AE_OS_WINDOWS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <cerrno>
#include <csignal>
#endif

namespace Atlas::Capture {

    // The registered instance of this process with the identity of its process, which tells it apart from a later
    // process with the same id
    static std::optional<InstanceRegistry::Instance> self;
    static uint64_t selfProcessStart = 0;

    static fs::path GetDirectory() {

#ifdef AE_OS_WINDOWS
        auto base = std::getenv("LOCALAPPDATA");
        return fs::path(base ? base : ".") / "AtlasEngine" / "instances";
#else
        auto base = std::getenv("XDG_RUNTIME_DIR");
        return fs::path(base ? base : "/tmp") / "atlas-engine-instances";
#endif

    }

    static uint64_t GetProcessStart(uint32_t pid) {

#ifdef AE_OS_WINDOWS
        auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!process)
            return 0;

        DWORD exitCode = 0;
        FILETIME creation, exit, kernel, user;
        uint64_t start = 0;
        if (GetExitCodeProcess(process, &exitCode) && exitCode == STILL_ACTIVE &&
            GetProcessTimes(process, &creation, &exit, &kernel, &user))
            start = (uint64_t(creation.dwHighDateTime) << 32) | creation.dwLowDateTime;
        CloseHandle(process);
        return start;
#else
        // The id is enough here, kill(0) only checks whether the process exists
        return kill(pid_t(pid), 0) == 0 || errno == EPERM ? 1 : 0;
#endif

    }

    static void Write() {

        if (!self)
            return;

        json j = {
            { "pid", self->pid }, { "processStart", selfProcessStart }, { "executable", self->executable },
            { "args", self->args }, { "started", self->started }, { "activity", self->activity }, { "busy", self->busy },
            { "expectedEnd", self->expectedEnd ? json(*self->expectedEnd) : json() },
            { "waitingSince", self->waitingSince ? json(*self->waitingSince) : json() }
        };

        std::error_code error;
        auto directory = GetDirectory();
        fs::create_directories(directory, error);
        auto path = directory / (std::to_string(self->pid) + ".json");
        auto temporary = path;
        temporary += ".tmp";
        {
            std::ofstream stream(temporary);
            stream << j.dump();
        }
        // Fails on Windows while another process reads the file, the order of the GPU queue relies on the write
        for (int32_t attempt = 0; attempt < 20; attempt++) {
            fs::rename(temporary, path, error);
            if (!error)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

    }

    void InstanceRegistry::Register(const std::vector<std::string>& args) {

        self = Instance {
            .pid = CurrentProcessId(),
            .executable = args.empty() ? std::string() : fs::path(args.front()).filename().string(),
            .args = args.size() > 1 ? std::vector<std::string>(args.begin() + 1, args.end()) : std::vector<std::string>(),
            .started = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(),
            .activity = "running"
        };
        selfProcessStart = GetProcessStart(self->pid);
        Write();

    }

    void InstanceRegistry::Unregister() {

        if (!self)
            return;

        std::error_code error;
        fs::remove(GetDirectory() / (std::to_string(self->pid) + ".json"), error);
        self.reset();

    }

    void InstanceRegistry::SetActivity(const std::string& activity, bool busy, std::optional<int64_t> expectedEnd) {

        if (!self)
            return;

        self->activity = activity;
        self->busy = busy;
        self->expectedEnd = expectedEnd;
        self->waitingSince.reset();
        Write();

    }

    std::vector<InstanceRegistry::Instance> InstanceRegistry::GetInstances() {

        std::vector<Instance> instances;
        std::error_code error;
        for (const auto& entry : fs::directory_iterator(GetDirectory(), error)) {
            if (entry.path().extension() != ".json")
                continue;

            json j;
            {
                std::ifstream stream(entry.path());
                j = json::parse(stream, nullptr, false);
            }
            if (j.is_discarded() || !j.contains("pid"))
                continue;

            Instance instance;
            instance.pid = j["pid"].get<uint32_t>();
            auto start = GetProcessStart(instance.pid);
            if (start == 0 || start != j.value("processStart", start)) {
                fs::remove(entry.path(), error);
                continue;
            }

            instance.executable = j.value("executable", std::string());
            instance.args = j.value("args", std::vector<std::string>());
            instance.started = j.value("started", int64_t(0));
            instance.activity = j.value("activity", std::string());
            instance.busy = j.value("busy", true);
            if (j.contains("expectedEnd") && j["expectedEnd"].is_number())
                instance.expectedEnd = j["expectedEnd"].get<int64_t>();
            if (j.contains("waitingSince") && j["waitingSince"].is_number())
                instance.waitingSince = j["waitingSince"].get<int64_t>();
            instances.push_back(instance);
        }

        return instances;

    }

    std::vector<InstanceRegistry::Instance> InstanceRegistry::GetBusyInstances() {

        auto instances = GetInstances();
        std::erase_if(instances, [](const Instance& instance) { return !instance.busy || instance.pid == CurrentProcessId(); });
        return instances;

    }

    std::vector<InstanceRegistry::Instance> InstanceRegistry::GetInstancesAhead(const Instance& waiter, const std::vector<Instance>& instances) {

        std::vector<Instance> ahead;
        for (const auto& instance : instances) {
            if (instance.pid == waiter.pid)
                continue;
            // A later waiter which claimed the GPU before it saw this one backs off in its next step
            auto isAhead = instance.waitingSince && waiter.waitingSince ?
                std::pair(*instance.waitingSince, instance.pid) < std::pair(*waiter.waitingSince, waiter.pid) : instance.busy;
            if (isAhead)
                ahead.push_back(instance);
        }
        return ahead;

    }

    void InstanceRegistry::QueueForGpu(const std::string& activity) {

        if (!self)
            return;

        self->activity = activity;
        self->busy = false;
        self->expectedEnd.reset();
        self->waitingSince = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        Write();

    }

    bool InstanceRegistry::PollGpu(std::vector<Instance>& ahead) {

        if (!self) {
            ahead = GetBusyInstances();
            return ahead.empty();
        }

        ahead = GetInstancesAhead(*self, GetInstances());
        if (!ahead.empty()) {
            if (self->busy) {
                self->busy = false;
                Write();
            }
            return false;
        }

        if (self->busy)
            return true;

        self->busy = true;
        Write();
        return false;

    }

    std::string InstanceRegistry::Describe(const std::vector<Instance>& instances) {

        std::string description;
        for (const auto& instance : instances)
            description += (description.empty() ? "" : ", ") + instance.executable + " (process " + std::to_string(instance.pid) +
                "): " + instance.activity;
        return description;

    }

}
