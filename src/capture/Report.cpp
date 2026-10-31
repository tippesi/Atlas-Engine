#include "Report.h"

#include "graphics/GraphicsDevice.h"
#include "graphics/Instance.h"
#include "graphics/MemoryManager.h"
#include "graphics/Profiler.h"
#include "resource/ResourceManager.h"
#include "texture/Texture2D.h"

#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <exception>
#if __has_include(<stacktrace>)
#include <stacktrace>
#endif

#ifdef AE_OS_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace Atlas::Capture {

    // The report the crash handlers write into
    static Report* crashReport = nullptr;

    static uint32_t CurrentProcessId() {

#ifdef AE_OS_WINDOWS
        return uint32_t(::GetCurrentProcessId());
#else
        return uint32_t(getpid());
#endif

    }

    static bool IsProcessRunning(uint32_t processId) {

#ifdef AE_OS_WINDOWS
        auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
        if (!process)
            return false;
        DWORD code = 0;
        bool running = GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
        CloseHandle(process);
        return running;
#else
        return kill(pid_t(processId), 0) == 0;
#endif

    }

    // Remove only outputs listed in the previous report
    static void RemovePreviousOutputs(const fs::path& directory) {

        std::ifstream stream(directory / "report.json");
        if (!stream.is_open())
            return;

        auto previousReport = json::parse(stream, nullptr, false);
        stream.close();
        if (previousReport.is_discarded() || !previousReport.contains("outputs"))
            return;

        std::error_code error;
        for (const auto& output : previousReport["outputs"]) {
            auto path = (directory / output.get<std::string>()).lexically_normal();
            if (path.lexically_relative(directory).string().starts_with(".."))
                continue;

            fs::remove(path, error);
            // Directories of strips and variants
            auto parent = path.parent_path();
            if (parent != directory && fs::is_empty(parent, error))
                fs::remove(parent, error);
        }

    }

    // Device memory of the images and buffers alive at the end of a job: textures (images with mip maps, by their
    // resource path), other images (render targets, maps) and buffers by usage, plus the budget of the device heaps
    static json GpuMemoryReport() {

        auto device = Graphics::GraphicsDevice::DefaultDevice;
        auto allocator = device->memoryManager->allocator;
        constexpr double megabyte = 1024.0 * 1024.0;

        auto allocationSize = [&](VmaAllocation allocation) -> VkDeviceSize {
            if (allocation == VK_NULL_HANDLE)
                return 0;
            VmaAllocationInfo info;
            vmaGetAllocationInfo(allocator, allocation, &info);
            return info.size;
        };

        std::unordered_map<Graphics::Image*, std::string> texturePaths;
        for (const auto& texture : ResourceManager<Texture::Texture2D>::GetResources())
            if (texture.IsLoaded() && texture->image)
                texturePaths[texture->image.get()] = texture.GetResource()->path;

        struct Entry { std::string name; VkDeviceSize size; };
        std::vector<Entry> textures, images, buffers;
        std::map<std::string, VkDeviceSize> bufferUsage;
        VkDeviceSize textureBytes = 0, imageBytes = 0, bufferBytes = 0;
        {
            std::lock_guard lock(device->images.mutex);
            for (const auto& image : device->images.data) {
                auto size = allocationSize(image->allocation);
                auto description = std::to_string(image->width) + "x" + std::to_string(image->height) +
                    (image->layers > 1 ? "x" + std::to_string(image->layers) : "") + " format " + std::to_string(int32_t(image->format)) +
                    ", " + std::to_string(image->mipLevels) + " mips";
                auto path = texturePaths.find(image.get());
                if (path != texturePaths.end()) {
                    textures.push_back({ path->second + " (" + description + ")", size });
                    textureBytes += size;
                }
                else {
                    images.push_back({ description, size });
                    imageBytes += size;
                }
            }
        }
        {
            std::lock_guard lock(device->buffers.mutex);
            for (const auto& buffer : device->buffers.data) {
                auto size = allocationSize(buffer->allocation);
                std::string usage = buffer->usageFlags & VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR ? "accelerationStructure" :
                    buffer->usageFlags & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT ? "vertex" :
                    buffer->usageFlags & VK_BUFFER_USAGE_INDEX_BUFFER_BIT ? "index" :
                    buffer->usageFlags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ? "storage" :
                    buffer->usageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT ? "uniform" : "other";
                bufferUsage[usage] += size;
                buffers.push_back({ usage + " " + std::to_string(buffer->size) + " bytes", size });
                bufferBytes += size;
            }
        }

        auto largest = [&](std::vector<Entry>& entries) {
            std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.size > b.size; });
            json list = json::array();
            for (size_t i = 0; i < std::min(entries.size(), size_t(25)); i++)
                list.push_back({ { "name", entries[i].name }, { "megabytes", double(entries[i].size) / megabyte } });
            return list;
        };

        json usage = json::object();
        for (const auto& [name, size] : bufferUsage)
            usage[name] = double(size) / megabyte;

        VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
        vmaGetHeapBudgets(allocator, budgets);
        const VkPhysicalDeviceMemoryProperties* properties;
        vmaGetMemoryProperties(allocator, &properties);
        json heaps = json::array();
        for (uint32_t i = 0; i < properties->memoryHeapCount; i++) {
            if (!(properties->memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT))
                continue;
            heaps.push_back({ { "blockMegabytes", double(budgets[i].statistics.blockBytes) / megabyte },
                { "allocationMegabytes", double(budgets[i].statistics.allocationBytes) / megabyte },
                { "processUsageMegabytes", double(budgets[i].usage) / megabyte },
                { "budgetMegabytes", double(budgets[i].budget) / megabyte } });
        }

        return {
            { "textureMegabytes", double(textureBytes) / megabyte }, { "textureCount", textures.size() },
            { "otherImageMegabytes", double(imageBytes) / megabyte }, { "otherImageCount", images.size() },
            { "bufferMegabytes", double(bufferBytes) / megabyte }, { "bufferCount", buffers.size() },
            { "bufferMegabytesByUsage", usage }, { "deviceHeaps", heaps },
            { "largestTextures", largest(textures) }, { "largestOtherImages", largest(images) }, { "largestBuffers", largest(buffers) }
        };

    }

    void Report::InstallCrashHandlers() {

        crashReport = this;

        std::set_terminate([]() {

            std::string reason = "std::terminate";
            if (auto exception = std::current_exception()) {
                try {
                    std::rethrow_exception(exception);
                }
                catch (const std::exception& e) {
                    reason += " with uncaught exception: " + std::string(e.what());
                }
                catch (...) {
                    reason += " with an unknown exception";
                }
            }
            crashReport->WriteCrash(reason);
            std::_Exit(crashReport->written ? crashReport->exitCode : 3);

        });

        std::signal(SIGABRT, [](int32_t) {

            crashReport->WriteCrash("abort");
            std::_Exit(crashReport->written ? crashReport->exitCode : 3);

        });

#ifdef AE_OS_WINDOWS
        SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* info) -> LONG {

            char code[32];
            std::snprintf(code, sizeof(code), "0x%08lX", info->ExceptionRecord->ExceptionCode);
            crashReport->WriteCrash(std::string("unhandled exception ") + code);
            std::_Exit(crashReport->written ? crashReport->exitCode : 3);

        });
#endif

    }

    void Report::Reset(const Job& job) {

        jobName = job.name;
        outputDirectory = job.outputDirectory;
        showWindow = job.showWindow;

        data = json();
        errors.clear();
        warnings.clear();
        expectations = json::array();
        outputs = json::array();
        logErrorCount = 0;
        exitCode = 0;
        written = false;
        reportPath.clear();
        cpuFrameTimes.clear();

        std::lock_guard lock(validationMutex);
        validationMessages.clear();

    }

    std::string Report::Begin() {

        fs::create_directories(outputDirectory);
        runId = std::to_string(std::chrono::system_clock::now().time_since_epoch() / std::chrono::seconds(1)) + "-" +
            std::to_string(CurrentProcessId());

        // Prevent concurrent runs from mixing outputs
        auto path = outputDirectory / "capture.lock";
        bool locked = false;
        for (int32_t attempt = 0; attempt < 2 && !locked; attempt++) {
            // Exclusive creation, only one process can succeed
            if (auto file = std::fopen(path.string().c_str(), "wx")) {
                auto content = json { { "pid", CurrentProcessId() }, { "job", jobName }, { "runId", runId } }.dump();
                std::fputs(content.c_str(), file);
                std::fclose(file);
                lockPath = path;
                locked = true;
                break;
            }

            std::ifstream stream(path);
            auto lock = json::parse(stream, nullptr, false);
            stream.close();
            auto processId = lock.is_object() ? lock.value("pid", uint32_t(0)) : uint32_t(0);
            if (processId != 0 && IsProcessRunning(processId))
                return "The output directory " + outputDirectory.string() + " is used by capture process " +
                    std::to_string(processId) + " (job '" + lock.value("job", std::string()) + "', run " +
                    lock.value("runId", std::string()) + "). Wait for it or choose another directory with --out";

            // Left behind by a process that crashed or was killed
            std::error_code error;
            fs::remove(path, error);
        }
        if (!locked)
            return "Couldn't create the lock file " + path.string();

        // Clear stale outputs before recording the running state
        RemovePreviousOutputs(outputDirectory);
        reportPath = outputDirectory / "report.json";
        json running = { { "job", jobName }, { "runId", runId }, { "status", "running" }, { "pid", CurrentProcessId() } };
        WriteFileAtomic(reportPath, running.dump(2));
        logFile.open(outputDirectory / "log.txt");

        return {};

    }

    void Report::End() {

        ReleaseLock();
        logFile.close();

    }

    void Report::Write(const json& result) {

        data.update(result);
        data["outputs"] = outputs;
        data["gpuMemory"] = GpuMemoryReport();

        WriteJson("report.json", data);
        written = true;

    }

    void Report::Print(const std::string& message) {

        std::printf("[capture] %s\n", message.c_str());
        std::fflush(stdout);

        if (showWindow) {
            auto color = vec4(0.85f, 0.85f, 0.85f, 1.0f);
            if (message.starts_with("ERROR") || message.starts_with("FAILED"))
                color = vec4(1.0f, 0.4f, 0.35f, 1.0f);
            else if (message.starts_with("WARNING"))
                color = vec4(1.0f, 0.8f, 0.3f, 1.0f);
            else if (message.starts_with("Status: passed"))
                color = vec4(0.45f, 0.9f, 0.45f, 1.0f);
            outputLines.push_back({ message, color });
            if (outputLines.size() > 200)
                outputLines.pop_front();
        }

        if (logFile.is_open())
            logFile << "[capture] " << message << "\n";

    }

    void Report::Warn(const std::string& message) {

        warnings.push_back(message);
        Print("WARNING: " + message);

    }

    void Report::Error(const std::string& message) {

        errors.push_back(message);
        Print("ERROR: " + message);

    }

    void Report::WriteJson(const std::string& fileName, const json& j) {

        auto path = outputDirectory / fileName;
        fs::create_directories(path.parent_path());

        if (!WriteFileAtomic(path, j.dump(2)))
            Warn("Couldn't write " + path.string());

        outputs.push_back(fileName);

    }

    void Report::AddOutput(const std::string& path) {

        outputs.push_back(path);

    }

    const json& Report::GetOutputs() const {

        return outputs;

    }

    std::vector<Log::Entry> Report::CollectLog() {

        auto entries = Log::GetEntriesSince(logIndex);
        logIndex += entries.size();

        for (const auto& entry : entries) {
            std::string type = entry.type == Log::TYPE_ERROR ? "ERROR" : (entry.type == Log::TYPE_WARNING ? "WARNING" : "INFO");
            if (logFile.is_open())
                logFile << "[" << FormatFloat(entry.time, 2) << "] " << type << ": " << entry.message << "\n";

            if (entry.type == Log::TYPE_ERROR) {
                logErrorCount++;
                std::printf("[engine] ERROR: %s\n", entry.message.c_str());
            }
        }

        if (logFile.is_open())
            logFile.flush();

        return entries;

    }

    void Report::SkipLog() {

        logIndex = Log::GetEntryCount();

    }

    void Report::BeginFrame(int32_t profileFrames) {

        auto now = std::chrono::steady_clock::now();
        if (profileFrames > 0 && frameStart.time_since_epoch().count() != 0) {
            currentCpuTimes.frame = std::chrono::duration<float, std::milli>(now - frameStart).count();
            cpuFrameTimes.push_back(currentCpuTimes);
            if (cpuFrameTimes.size() > size_t(profileFrames))
                cpuFrameTimes.pop_front();
        }
        frameStart = now;
        currentCpuTimes = {};

    }

    float Report::GetFrameMilliseconds() const {

        return std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - frameStart).count();

    }

    bool Report::EnableValidation() {

        if (validationMessenger != VK_NULL_HANDLE)
            return true;

        auto instance = Graphics::Instance::DefaultInstance;
        if (!instance->extendedValidationEnabled)
            return false;

        VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = ValidationCallback;
        createInfo.pUserData = this;
        VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance->GetNativeInstance(), &createInfo, nullptr, &validationMessenger))

        Print("Validation: GPU assisted, synchronization and best practices validation enabled");
        return true;

    }

    bool Report::IsValidationEnabled() const {

        return validationMessenger != VK_NULL_HANDLE;

    }

    void Report::DestroyValidation() {

        if (validationMessenger != VK_NULL_HANDLE)
            vkDestroyDebugUtilsMessengerEXT(Graphics::Instance::DefaultInstance->GetNativeInstance(), validationMessenger, nullptr);
        validationMessenger = VK_NULL_HANDLE;

    }

    void Report::WriteProfile(int32_t frames) {

        auto threads = Graphics::Profiler::GetQueriesAverage(uint32_t(frames));

        json queries = json::array();
        std::function<void(const Graphics::Profiler::Query&, const std::string&, int32_t)> addQuery =
            [&](const Graphics::Profiler::Query& query, const std::string& parent, int32_t depth) {

            auto path = parent.empty() ? query.name : parent + " / " + query.name;
            auto milliseconds = double(query.timer.elapsedTime) / 1000000.0;
            queries.push_back({ { "query", path }, { "depth", depth }, { "ms", Round(milliseconds, 4) } });
            for (const auto& child : query.children)
                addQuery(child, path, depth + 1);

        };

        json profile;
        profile["frames"] = frames;
        for (const auto& thread : threads) {
            queries = json::array();
            double total = 0.0;
            for (const auto& query : thread.queries) {
                total += double(query.timer.elapsedTime) / 1000000.0;
                addQuery(query, "", 0);
            }
            profile["threads"].push_back({ { "thread", thread.name }, { "ms", Round(total, 4) }, { "queries", queries } });

            Print("GPU " + thread.name + ": " + FormatFloat(float(total), 3) + " ms per frame");
            for (const auto& query : queries)
                if (query["depth"].get<int32_t>() <= 1 && query["ms"].get<double>() >= 0.05)
                    Print("  " + FormatFloat(query["ms"].get<float>(), 3) + " ms  " + query["query"].get<std::string>());
        }

        if (!cpuFrameTimes.empty()) {
            auto average = [&](float CpuFrameTimes::* member) {
                double sum = 0.0;
                for (const auto& times : cpuFrameTimes)
                    sum += times.*member;
                return Round(sum / double(cpuFrameTimes.size()), 4);
            };
            float maxFrame = 0.0f;
            for (const auto& times : cpuFrameTimes)
                maxFrame = std::max(maxFrame, times.frame);
            profile["cpu"] = { { "frames", cpuFrameTimes.size() }, { "frameMs", average(&CpuFrameTimes::frame) },
                { "maxFrameMs", Round(maxFrame, 4) }, { "waitForGpuMs", average(&CpuFrameTimes::wait) },
                { "sceneUpdateMs", average(&CpuFrameTimes::update) }, { "renderMs", average(&CpuFrameTimes::render) } };
            Print("CPU frame " + FormatFloat(float(profile["cpu"]["frameMs"].get<double>()), 3) + " ms: scene update " +
                FormatFloat(float(profile["cpu"]["sceneUpdateMs"].get<double>()), 3) + " ms, render " +
                FormatFloat(float(profile["cpu"]["renderMs"].get<double>()), 3) + " ms, waiting for the GPU " +
                FormatFloat(float(profile["cpu"]["waitForGpuMs"].get<double>()), 3) + " ms");
        }

        WriteJson("profile.json", profile);

    }

    void Report::WriteValidation() {

        std::lock_guard lock(validationMutex);

        // Errors first, then the most frequent messages
        std::vector<std::pair<std::string, ValidationMessage>> sorted(validationMessages.begin(), validationMessages.end());
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {

            bool aError = a.second.severity == "error", bError = b.second.severity == "error";
            if (aError != bError)
                return aError;
            return a.second.count > b.second.count;

        });

        json messages = json::array();
        std::map<std::string, int32_t> counts = { { "error", 0 }, { "warning", 0 }, { "performance", 0 } };
        for (const auto& [id, message] : sorted) {
            messages.push_back({ { "id", id }, { "severity", message.severity }, { "count", message.count },
                { "message", message.message } });
            counts[message.severity]++;
        }

        json summary = { { "errors", counts["error"] }, { "warnings", counts["warning"] },
            { "performance", counts["performance"] } };
        WriteJson("validation.json", { { "summary", summary }, { "messages", messages } });
        data["validation"] = summary;

        Print("Validation: " + std::to_string(counts["error"]) + " errors, " + std::to_string(counts["warning"]) +
            " warnings, " + std::to_string(counts["performance"]) + " performance warnings (distinct ids)");
        for (const auto& [id, message] : sorted) {
            if (message.severity == "error")
                Print("VALIDATION " + id + " (" + std::to_string(message.count) + "x)");
        }

    }

    VkBool32 Report::ValidationCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* data, void* userData) {

        auto report = static_cast<Report*>(userData);
        auto id = data->pMessageIdName ? std::string(data->pMessageIdName) : std::to_string(data->messageIdNumber);

        std::lock_guard lock(report->validationMutex);
        auto& message = report->validationMessages[id];
        if (message.count++ == 0) {
            if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
                message.severity = "error";
            else
                message.severity = type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT ? "performance" : "warning";
            // Contains the involved objects with their debug names
            message.message = data->pMessage;
        }

        return VK_FALSE;

    }

    void Report::ReleaseLock() {

        if (lockPath.empty())
            return;
        std::error_code error;
        fs::remove(lockPath, error);
        lockPath.clear();

    }

    void Report::WriteCrash(const std::string& reason) {

        auto message = "[capture] CRASH: " + reason;
#if defined(__cpp_lib_stacktrace) && __cpp_lib_stacktrace >= 202011L
        message += "\nStack trace:\n" + std::to_string(std::stacktrace::current());
#endif
        std::fprintf(stderr, "%s\n", message.c_str());
        std::fprintf(stdout, "%s\n", message.c_str());
        std::fflush(stderr);
        std::fflush(stdout);

        // The result of a job that finished is kept, e.g. when the shutdown crashes
        if (!written && !reportPath.empty()) {
            json report = { { "job", jobName }, { "runId", runId }, { "status", "crashed" }, { "exitCode", 3 },
                { "reason", reason } };
            WriteFileAtomic(reportPath, report.dump(2));
        }
        ReleaseLock();

    }

}
