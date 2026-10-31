#pragma once

#include "Job.h"
#include "CaptureRenderer.h"

#include "Log.h"
#include "graphics/Common.h"

#include <chrono>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>

namespace Atlas::Capture {

    // The results of a job in its output directory: report.json, log.txt and the other JSON outputs, the
    // printed output, the profile and the validation messages. Writes a report for crashes as well
    class Report {

    public:
        // CPU milliseconds of a frame: whole frame, waiting for the GPU, scene update, render
        struct CpuFrameTimes {

            float frame = 0.0f;
            float wait = 0.0f;
            float update = 0.0f;
            float render = 0.0f;

        };

        // Crashes write a report with the status "crashed" and exit with code 3
        void InstallCrashHandlers();

        // Clears the report for a job, it is written to the output directory of the job
        void Reset(const Job& job);

        // Starts the report in the output directory: takes the lock of the directory, removes the outputs of the
        // previous run, writes the running state and opens log.txt. Returns an error if another process uses the directory
        std::string Begin();

        // Releases the lock of the output directory and closes log.txt
        void End();

        // Writes report.json with the result, the outputs and the GPU memory. The result can't change anymore,
        // even if the shutdown crashes
        void Write(const json& result);

        void Print(const std::string& message);

        void Warn(const std::string& message);

        void Error(const std::string& message);

        // Written to the output directory and listed as an output
        void WriteJson(const std::string& fileName, const json& j);

        // A path relative to the output directory
        void AddOutput(const std::string& path);

        const json& GetOutputs() const;

        // Engine log entries since the last call, written to log.txt
        std::vector<Log::Entry> CollectLog();

        // Engine log entries before this are not reported
        void SkipLog();

        // Measures the CPU times of the last frames, only while the profile is enabled
        void BeginFrame(int32_t profileFrames);

        // Since the frame began
        float GetFrameMilliseconds() const;

        // Returns false if the validation layers aren't available
        bool EnableValidation();

        // Writes the GPU timings averaged over the last frames and the CPU times to profile.json
        void WriteProfile(int32_t frames);

        // Writes the validation messages grouped by their id to validation.json
        void WriteValidation();

        bool IsValidationEnabled() const;

        void DestroyValidation();

        // Fields of report.json besides the result, e.g. the edit
        json data;

        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        json expectations = json::array();
        int32_t logErrorCount = 0;

        int32_t exitCode = 0;
        std::string runId;

        CpuFrameTimes currentCpuTimes;

        // Lines of the output panel of a shown window
        std::deque<CaptureRenderer::TextLine> outputLines;
        bool showWindow = true;

    private:
        struct ValidationMessage {

            std::string severity;
            std::string message;
            int32_t count = 0;

        };

        static VKAPI_ATTR VkBool32 VKAPI_CALL ValidationCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* data, void* userData);

        void ReleaseLock();

        void WriteCrash(const std::string& reason);

        std::string jobName;
        fs::path outputDirectory;
        fs::path lockPath;
        // Set once the report of a job began, the crash handlers write into it
        fs::path reportPath;
        json outputs = json::array();
        bool written = false;

        std::ofstream logFile;
        size_t logIndex = 0;

        std::deque<CpuFrameTimes> cpuFrameTimes;
        std::chrono::steady_clock::time_point frameStart;

        // Message id -> message, the validation layers call back from any thread
        VkDebugUtilsMessengerEXT validationMessenger = VK_NULL_HANDLE;
        std::map<std::string, ValidationMessage> validationMessages;
        std::mutex validationMutex;

    };

}
