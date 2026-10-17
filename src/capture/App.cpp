#include "App.h"
#include "SceneEditor.h"

#include "Serializer.h"
#include "loader/ImageLoader.h"
#include "Log.h"
#include "resource/ResourceManager.h"
#include "scripting/Script.h"
#include "scene/components/LuaScriptComponent.h"
#include "events/EventManager.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/Instance.h"
#include "graphics/Profiler.h"

#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <exception>
#include <stacktrace>

#ifdef AE_OS_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
// Collides with Loader::ImageLoader::LoadImage
#undef LoadImage
#else
#include <unistd.h>
#endif

const Atlas::EngineConfig Atlas::EngineInstance::engineConfig = {
    .assetDirectory = "../../data",
    .shaderDirectory = "shader"
};

namespace Atlas::Capture {

    using namespace Scene::Components;

    static int32_t jobExitCode = 0;
    // Preserve the job result if shutdown crashes after the report was written
    static bool reportWritten = false;

    // Known to the crash handlers, which write the report of a crashed run
    static std::string runId;
    static std::string runJobName;
    static fs::path reportPath;
    static fs::path lockPath;

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

    // Write next to the target, then rename; a failed rename leaves the target unchanged
    static bool WriteFileAtomic(const fs::path& path, const std::string& content) {

        auto temporary = path;
        temporary += ".tmp";
        {
            std::ofstream stream(temporary, std::ios::binary);
            if (!stream.is_open())
                return false;
            stream << content;
        }

        std::error_code error;
        fs::rename(temporary, path, error);
        return !error;

    }

    // Prevent concurrent runs from mixing outputs
    static std::string AcquireLock(const fs::path& directory) {

        auto path = directory / "capture.lock";
        for (int32_t attempt = 0; attempt < 2; attempt++) {
            // Exclusive creation, only one process can succeed
            if (auto file = std::fopen(path.string().c_str(), "wx")) {
                auto content = json{ { "pid", CurrentProcessId() }, { "job", runJobName }, { "runId", runId } }.dump();
                std::fputs(content.c_str(), file);
                std::fclose(file);
                lockPath = path;
                return {};
            }

            std::ifstream stream(path);
            auto lock = json::parse(stream, nullptr, false);
            stream.close();
            auto processId = lock.is_object() ? lock.value("pid", uint32_t(0)) : uint32_t(0);
            if (processId != 0 && IsProcessRunning(processId))
                return "The output directory " + directory.string() + " is used by capture process " +
                    std::to_string(processId) + " (job '" + lock.value("job", std::string()) + "', run " +
                    lock.value("runId", std::string()) + "). Wait for it or choose another directory with --out";

            // Left behind by a process that crashed or was killed
            std::error_code error;
            fs::remove(path, error);
        }

        return "Couldn't create the lock file " + path.string();

    }

    static void ReleaseLock() {

        if (lockPath.empty())
            return;
        std::error_code error;
        fs::remove(lockPath, error);
        lockPath.clear();

    }

    static void WriteCrashReport(const std::string& reason) {

        if (reportWritten || reportPath.empty())
            return;

        json report = { { "job", runJobName }, { "runId", runId }, { "status", "crashed" }, { "exitCode", 3 },
            { "reason", reason } };
        WriteFileAtomic(reportPath, report.dump(2));

    }

    struct ViewAngles {

        float azimuth;
        float elevation;

    };

    static ViewAngles GetViewAngles(const std::string& view) {

        // Azimuth 0 places the camera on the +Z side looking towards -Z
        static const std::map<std::string, ViewAngles> views = {
            { "front", { 0.0f, 15.0f } },
            { "right", { 90.0f, 15.0f } },
            { "back", { 180.0f, 15.0f } },
            { "left", { 270.0f, 15.0f } },
            { "top", { 0.0f, 89.0f } },
            { "iso", { 45.0f, 35.0f } },
            // Almost at ground level, best to see whether something floats
            { "low", { 0.0f, 2.0f } },
            { "lowSide", { 90.0f, 2.0f } },
        };

        auto it = views.find(view);
        if (it == views.end())
            throw std::runtime_error("Unknown view '" + view + "' (front, right, back, left, top, iso, low, lowSide)");

        return it->second;

    }

    // Record crashes in the job output
    static void PrintCrash(const std::string& reason) {

        auto message = "[capture] CRASH: " + reason + "\nStack trace:\n" + std::to_string(std::stacktrace::current());
        std::fprintf(stderr, "%s\n", message.c_str());
        std::fprintf(stdout, "%s\n", message.c_str());
        std::fflush(stderr);
        std::fflush(stdout);

        WriteCrashReport(reason);
        ReleaseLock();

    }

    static void InstallCrashHandlers() {

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
            PrintCrash(reason);
            std::_Exit(reportWritten ? jobExitCode : 3);

        });

        std::signal(SIGABRT, [](int32_t) {

            PrintCrash("abort");
            std::_Exit(reportWritten ? jobExitCode : 3);

        });

#ifdef AE_OS_WINDOWS
        SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* info) -> LONG {

            char code[32];
            std::snprintf(code, sizeof(code), "0x%08lX", info->ExceptionRecord->ExceptionCode);
            PrintCrash(std::string("unhandled exception ") + code);
            std::_Exit(reportWritten ? jobExitCode : 3);

        });
#endif

    }

    // Keep scripts active with a minimal time step while waiting for real-time playback
    static constexpr float freezeDeltaTime = 1e-5f;

    static bool RunCompose(const std::vector<std::string>& args) {

        if (args.size() < 3 || args[1] != "--compose")
            return false;

        auto output = ResolveUserPath(args[2]);
        std::string title;
        int32_t columns = 0;
        std::vector<Ref<Image>> images;
        std::vector<std::string> labels;

        for (size_t i = 3; i < args.size(); i++) {
            if (args[i] == "--title" && i + 1 < args.size()) {
                title = args[++i];
                continue;
            }
            if (args[i] == "--columns" && i + 1 < args.size()) {
                columns = std::stoi(args[++i]);
                continue;
            }

            auto separator = args[i].find('=');
            auto label =
                separator == std::string::npos ? fs::path(args[i]).stem().string() : args[i].substr(0, separator);
            auto path = ResolveUserPath(separator == std::string::npos ? args[i] : args[i].substr(separator + 1));

            auto image = Loader::ImageLoader::LoadImage<uint8_t>(path.string(), false, 4);
            if (!image || !image->HasData()) {
                std::printf("Couldn't load %s\n", path.string().c_str());
                jobExitCode = 2;
                return true;
            }
            images.push_back(image);
            labels.push_back(label);
        }

        auto sheet = ComposeSheet(images, labels, columns, title);
        jobExitCode = WritePng(sheet, output) ? 0 : 2;
        std::printf("[capture] Composed %zu images into %s\n", images.size(), output.string().c_str());
        return true;

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

    static std::string FormatTime(float time) {

        return "t=" + FormatFloat(time, 2) + "s";

    }

    void App::LoadContent() {

        startTime = std::chrono::steady_clock::now();
        InstallCrashHandlers();

        if (RunCompose(args)) {
            phase = Phase::Done;
            Exit(jobExitCode);
            return;
        }

        try {
            job = ParseCommandLine(args);
        }
        catch (const std::exception& e) {
            std::string message = e.what();
            std::printf("%s\n", message.c_str());
            jobExitCode = message.starts_with("Usage:") ? 0 : 2;
            phase = Phase::Done;
            Exit(jobExitCode);
            return;
        }

        fs::create_directories(job.outputDirectory);

        runId = std::to_string(std::chrono::system_clock::now().time_since_epoch() / std::chrono::seconds(1)) + "-" +
            std::to_string(CurrentProcessId());
        runJobName = job.name;
        if (auto error = AcquireLock(job.outputDirectory); !error.empty()) {
            std::printf("[capture] ERROR: %s\n", error.c_str());
            std::printf("[capture] Status: error (%s)\n", error.c_str());
            jobExitCode = 2;
            phase = Phase::Done;
            Exit(jobExitCode);
            return;
        }

        // Clear stale outputs before recording the running state
        RemovePreviousOutputs(job.outputDirectory);
        reportPath = job.outputDirectory / "report.json";
        WriteFileAtomic(reportPath,
            json{ { "job", job.name }, { "runId", runId }, { "status", "running" }, { "pid", CurrentProcessId() } }
                .dump(2));
        logFile.open(job.outputDirectory / "log.txt");

        if (job.showWindow) {
            font = CreateRef<Font>("font/roboto.ttf", 16.0f, 5);

            // The frame keeps its aspect ratio above the output panel, large captures are shown smaller
            auto panelHeight = CaptureRenderer::GetPanelHeight(font, outputLineCount + 1);
            auto scale = std::min({ 1.0f, 1600.0f / float(job.resolution.x), 900.0f / float(job.resolution.y) });
            window.SetSize(
                int32_t(float(job.resolution.x) * scale), int32_t(float(job.resolution.y) * scale + panelHeight));
            window.Show();
        }

        // The profiler is disabled by default in release builds
        if (job.profileFrames > 0)
            Graphics::Profiler::enable = true;

        Print("Job '" + job.name + "' with scene " + job.scene + " (run " + runId + ")");

        if (job.validation) {
            auto instance = Graphics::Instance::DefaultInstance;
            if (!instance->extendedValidationEnabled) {
                Fail("Validation layers aren't available, they are part of the Vulkan SDK");
                return;
            }

            VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            createInfo.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            createInfo.messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            createInfo.pfnUserCallback = ValidationCallback;
            createInfo.pUserData = this;
            VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance->GetNativeInstance(), &createInfo, nullptr, &validationMessenger))

            Print("Validation: GPU assisted, synchronization and best practices validation enabled");
        }
        Print("Output directory: " + job.outputDirectory.string());

        captureRenderer.Init(mainRenderer, job.resolution);
        overlay = std::make_unique<Overlay>();

        // Capture errors that happen while loading
        logIndex = Log::GetEntryCount();

        try {
            auto binary = fs::path(job.scene).extension() != ".json";
            scene = Serializer::DeserializeScene(job.scene, binary);
        }
        catch (const std::exception& e) {
            Fail("Couldn't load scene " + job.scene + ": " + e.what());
            return;
        }

        if (!scene) {
            Fail("Couldn't load scene " + job.scene);
            return;
        }

        captureCameraEntity = scene->CreateEntity();
        auto& camera = captureCameraEntity.AddComponent<CameraComponent>(
            60.0f, float(job.resolution.x) / float(job.resolution.y), 0.1f, 2000.0f);
        camera.isMain = false;

        // The mouse over a shown window must not turn the players of a running job
        playerInput.useMouse = false;

        checkRunner = std::make_unique<CheckRunner>(job.checks);
        if (job.track)
            tracker = std::make_unique<Tracker>(*job.track);

        // Load with the camera of the first shot such that streaming and probes start in the right area
        if (!job.shots.empty())
            activeCamera = job.shots.front().camera;
        else
            activeCamera = job.observer.value_or(CameraSpec {});

    }

    void App::UnloadContent() {

        if (phase != Phase::Done) {
            Fail("The window was closed before the job finished");
            Finish();
        }
        ReleaseLock();

        graphicsDevice->WaitForIdle();

        if (validationMessenger != VK_NULL_HANDLE)
            vkDestroyDebugUtilsMessengerEXT(
                Graphics::Instance::DefaultInstance->GetNativeInstance(), validationMessenger, nullptr);

        scene.reset();
        logFile.close();

    }

    void App::Update(float deltaTime) {

        if (phase == Phase::Done)
            return;

        graphicsDevice->WaitForPreviousFrameSubmission();

        try {
            ProcessReadback();
            CollectLog();

            frameCount++;

            switch (phase) {
            case Phase::Loading: {
                UpdateSceneWithCaptureCamera(*activeCamera);
                auto elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
                if (IsSceneReady()) {
                    for (const auto& mesh : scene->GetMeshes())
                        if (mesh.IsValid() && !mesh.IsLoaded())
                            Warn("Mesh " + mesh.GetResource()->path + " failed to load");

                    loadSeconds = elapsed;
                    Print("Scene ready after " + FormatFloat(loadSeconds, 1) + "s (" + std::to_string(frameCount) +
                        " frames, " + std::to_string(scene->GetEntityCount() - 1) + " entities)");
                    phase = Phase::Prepare;
                }
                else if (elapsed > job.loadTimeout) {
                    Fail("Scene didn't finish loading within " + FormatFloat(job.loadTimeout, 0) + "s");
                }
                break;
            }
            case Phase::Prepare:
                Prepare();
                break;
            case Phase::EditSave:
                FinishEdit();
                break;
            case Phase::Shots:
                UpdateShots();
                break;
            case Phase::Play:
                UpdatePlay();
                break;
            case Phase::Finish:
                if (!HasPendingCaptures())
                    Finish();
                break;
            default:
                break;
            }
        }
        catch (const std::exception& e) {
            Fail(e.what());
        }

    }

    void App::Render(float deltaTime) {

        // While finishing the scene isn't updated anymore, rendering it without an update shows garbage
        auto finished = phase == Phase::Done || (phase == Phase::Finish && captureRequests.empty());
        if (finished || !scene) {
            // Scene updates can enqueue GPU work even when no scene frame is rendered
            if (scene)
                scene->WaitForAsyncWorkCompletion();
            PresentWindow();
            return;
        }

        std::vector<std::string> passes;
        for (const auto& request : captureRequests)
            for (const auto& pass : request.passes)
                if (std::find(passes.begin(), passes.end(), pass) == passes.end())
                    passes.push_back(pass);

        try {
            BuildOverlay();
            captureRenderer.Render(scene, passes, overlay->GetBatches());
        }
        catch (const std::exception& e) {
            // E.g. validation errors, which the engine throws in non-release builds
            Fail(std::string("Rendering failed: ") + e.what());
            captureRequests.clear();
            PresentWindow();
            return;
        }

        pendingReadbacks = std::move(captureRequests);
        captureRequests.clear();

        PresentWindow();

    }

    void App::PresentWindow() {

        if (!job.showWindow || !font) {
            captureRenderer.Present();
            return;
        }

        std::vector<CaptureRenderer::TextLine> lines = { { GetStatusLine(), vec4(0.55f, 0.8f, 1.0f, 1.0f) } };
        auto first = outputLines.size() > outputLineCount ? outputLines.size() - outputLineCount : 0;
        lines.insert(lines.end(), outputLines.begin() + first, outputLines.end());

        captureRenderer.Present(font, lines);

    }

    std::string App::GetStatusLine() const {

        std::string line = job.name + "  |  ";
        switch (phase) {
        case Phase::Loading:
            line += "loading " + job.scene;
            break;
        case Phase::Prepare:
        case Phase::EditSave:
            line += "preparing";
            break;
        case Phase::Shots:
            line += "shots";
            break;
        case Phase::Play:
            line += "playing  " + FormatFloat(playTime, 2) + " / " + FormatFloat(job.duration, 2) + " s";
            if (scene && scene->paused)
                line += "  (paused)";
            break;
        case Phase::Finish:
            line += "writing outputs";
            break;
        case Phase::Done:
            line += jobExitCode == 0 ? "passed" : jobExitCode == 1 ? "failed" : "error";
            break;
        }
        return line + "  |  frame " + std::to_string(frameCount);
    }

    void App::Fail(const std::string& message) {

        errors.push_back(message);
        fatal = true;
        Print("ERROR: " + message);

        if (phase != Phase::Done)
            phase = Phase::Finish;

    }

    void App::Warn(const std::string& message) {

        warnings.push_back(message);
        Print("WARNING: " + message);

    }

    void App::Print(const std::string& message) {

        std::printf("[capture] %s\n", message.c_str());
        std::fflush(stdout);

        if (job.showWindow) {
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

    void App::CollectLog() {

        auto entries = Log::GetEntriesSince(logIndex);
        logIndex += entries.size();

        for (const auto& entry : entries) {
            // Log conditions: a text without wildcards matches when the message contains it
            for (auto& trigger : triggers) {
                if (trigger.logMatched || playTime + 1e-4f < trigger.event.time || !trigger.event.when->contains("log"))
                    continue;
                auto pattern = (*trigger.event.when)["log"].get<std::string>();
                if (pattern.find_first_of("*?") == std::string::npos)
                    pattern = "*" + pattern + "*";
                trigger.logMatched = MatchPattern(pattern, entry.message);
            }

            std::string type =
                entry.type == Log::TYPE_ERROR ? "ERROR" : (entry.type == Log::TYPE_WARNING ? "WARNING" : "INFO");
            if (logFile.is_open())
                logFile << "[" << FormatFloat(entry.time, 2) << "] " << type << ": " << entry.message << "\n";

            if (entry.type == Log::TYPE_ERROR) {
                logErrorCount++;
                std::printf("[engine] ERROR: %s\n", entry.message.c_str());
                if (checkRunner && job.checks.logErrors && !MatchAnyPattern(job.checks.logIgnore, entry.message))
                    checkRunner->AddFailure({ "log", entry.message.substr(0, 160), playTime, entry.message });
            }
        }

        if (logFile.is_open())
            logFile.flush();

    }

    bool App::IsSceneReady() {

        // A few frames are needed to kick off the asynchronous work
        if (frameCount < 3)
            return false;

        // Meshes that failed to load count as done, they're reported once the scene is ready
        for (const auto& mesh : scene->GetMeshes()) {
            if (mesh.IsLoaded())
                continue;
            if (!mesh.IsValid() || !mesh.GetResource()->errorOnLoad)
                return false;
        }
        if (scene->terrain.IsValid() && !scene->terrain.IsLoaded())
            return false;

        // Ray tracing structures are built asynchronously, but not every device/scene builds them
        if (scene->rayTracingWorld && !scene->IsRtDataValid() && rtWaitFrames++ < 300)
            return false;

        return true;

    }

    void App::Prepare() {

        // The renderer expects a scene update in every frame
        UpdateSceneWithCaptureCamera(*activeCamera);

        if (!job.settings.empty())
            ApplySettings(*scene, job.settings);
        baseSettings = ExportSettings(*scene);

        EntitySet excluded = { captureCameraEntity };

        if (job.inspect) {
            auto inspection = InspectScene(*scene, job.inspect->filter, job.inspect->detail, excluded);
            WriteJson(job.inspect->file, inspection);
            Print("Inspection: " + std::to_string(inspection["entities"].size()) + " entities written to " +
                job.inspect->file);
        }

        if (!job.queries.empty()) {
            // Pixel picking uses the same camera setup as shots
            auto pickRay = [&](const json& cameraJson, vec2 pixel) {

                // The scene update computes the camera matrices, the simulation is paused at this point
                UpdateSceneWithCaptureCamera(ParseCamera(cameraJson));
                const auto& camera = captureCameraEntity.GetComponent<CameraComponent>();

                auto ray = Volume::Ray(captureRenderer.GetViewport(), camera, pixel + vec2(0.5f));
                return std::make_pair(ray.origin, ray.direction);

            };

            auto results = RunQueries(*scene, job.queries, pickRay);
            WriteJson("queries.json", results);
            queryResults = results;
            for (const auto& result : results) {
                auto text =
                    result.contains("error") ? "error: " + result["error"].get<std::string>() : result["result"].dump();
                if (text.size() > 400)
                    text = text.substr(0, 400) + "... (see queries.json)";
                Print("Query " + result["query"].dump() + " -> " + text);
            }
        }

        if (job.edit) {
            report["edit"]["before"] = InspectScene(*scene, {}, {}, excluded);
            report["edit"]["operations"] = ApplyEditOperations(*scene, job.edit->operations);
            Print("Applied " + std::to_string(job.edit->operations.size()) + " edit operations");
            phase = Phase::EditSave;
            return;
        }

        // Checks of a static scene run once, while playing they run continuously
        if (!job.play)
            checkRunner->Run(*scene, 0.0f);

        if (!job.shots.empty())
            phase = Phase::Shots;
        else if (job.play)
            StartPlay();
        else
            phase = Phase::Finish;

    }

    void App::FinishEdit() {

        // Propagate transforms and bounds before inspection
        UpdateSceneWithCaptureCamera(*activeCamera);
        if (++editFrames < 3)
            return;

        EntitySet excluded = { captureCameraEntity };
        auto& edit = report["edit"];

        auto after = InspectScene(*scene, {}, {}, excluded);
        auto diff = DiffInspections(edit["before"], after);
        edit.erase("before");
        WriteJson("edit_diff.json", diff);
        edit["diff"] = diff["summary"];
        Print("Edit diff: " + diff["summary"].get<std::string>() + " (see edit_diff.json)");

        checkRunner->Run(*scene, 0.0f);
        if (checkRunner->HasFailures()) {
            edit["saved"] = false;
            Fail("Checks failed after the edit, the scene was not saved");
            return;
        }

        // Edits which only changed material files leave the scene untouched, no need to rewrite it
        if (diff["changed"].empty() && diff["added"].empty() && diff["removed"].empty() && !job.edit->saveUnchanged) {
            edit["saved"] = false;
            Print("The scene itself didn't change, it is not saved");
            phase = job.shots.empty() ? Phase::Finish : Phase::Shots;
            return;
        }

        auto output = job.edit->output;
        if (output.empty()) {
            auto scenePath = fs::path(job.scene);
            output = (scenePath.parent_path() / (scenePath.stem().string() + ".edit" + scenePath.extension().string()))
                         .generic_string();
        }

        auto absoluteOutput = GetAssetDirectory() / output;
        if (output == job.scene && !job.edit->overwrite) {
            Fail("Refusing to overwrite the input scene " + output +
                ", set edit.overwrite = true or choose another output");
            return;
        }
        if (fs::exists(absoluteOutput) && output != job.scene && !job.edit->overwrite) {
            Fail("Output scene " + output + " already exists, set edit.overwrite = true to replace it");
            return;
        }

        if (fs::exists(absoluteOutput)) {
            auto timestamp = std::chrono::system_clock::now().time_since_epoch() / std::chrono::seconds(1);
            auto backup = absoluteOutput;
            backup += ".backup-" + std::to_string(timestamp);
            fs::copy_file(absoluteOutput, backup);
            edit["backup"] = backup.string();
            Print("Backup of the previous scene: " + backup.string());
        }

        // The capture camera must not end up in the scene, the editor does the same with its camera
        auto entityCount = scene->GetEntityCount() - 1;
        scene->DestroyEntity(captureCameraEntity);

        auto binary = fs::path(output).extension() != ".json";
        Serializer::SerializeScene(scene, output, false, binary);

        captureCameraEntity = scene->CreateEntity();
        captureCameraEntity
            .AddComponent<CameraComponent>(60.0f, float(job.resolution.x) / float(job.resolution.y), 0.1f, 2000.0f)
            .isMain = false;

        try {
            auto reloaded = Serializer::DeserializeScene(output, binary);
            auto reloadedCount = reloaded->GetEntityCount();
            edit["verified"] = reloadedCount == entityCount;
            if (reloadedCount != entityCount)
                Fail("Saved scene has " + std::to_string(reloadedCount) + " entities, expected " +
                    std::to_string(entityCount));
        }
        catch (const std::exception& e) {
            Fail("Saved scene " + output + " couldn't be loaded again: " + e.what());
            return;
        }

        edit["saved"] = true;
        edit["output"] = output;
        Print("Saved edited scene to data/" + output);

        if (phase == Phase::Finish)
            return;

        phase = job.shots.empty() ? Phase::Finish : Phase::Shots;

    }

    void App::UpdateShots() {

        auto variantCount = std::max(job.variants.size(), size_t(1));
        const auto& shot = job.shots[shotState.shot];
        auto views = shot.views.empty() ? std::vector<std::string> { "" } : shot.views;
        const auto& view = views[shotState.view];

        bool firstFrameOfShot = shotState.settledFrames == 0 && !shotState.requested;
        if (firstFrameOfShot && !job.variants.empty() && shotState.shot == 0 && shotState.view == 0) {
            const auto& variant = job.variants[shotState.variant];
            ApplySettings(*scene, baseSettings);
            ApplySettings(*scene, variant.settings);
            Print("Variant '" + variant.name + "'");
        }

        auto camera = shot.camera;
        if (!view.empty()) {
            camera.view = view;
            camera.azimuth.reset();
            camera.elevation.reset();
            if (camera.mode == "scene" || camera.mode == "manual")
                throw std::runtime_error(
                    "Shot '" + shot.name + "' has views, which require a focus or overview camera");
        }
        activeCamera = camera;

        UpdateSceneWithCaptureCamera(camera);

        auto settleFrames = shot.settleFrames >= 0 ? shot.settleFrames : job.settleFrames;
        if (!shotState.requested) {
            if (shotState.settledFrames++ < settleFrames)
                return;

            auto prefix =
                job.variants.empty() ? std::string() : SanitizeFileName(job.variants[shotState.variant].name) + "/";
            auto baseName = SanitizeFileName(shot.name) + (view.empty() ? "" : "_" + view);

            CaptureRequest request;
            request.path = job.outputDirectory / (prefix + baseName + ".png");
            request.passes = shot.passes;
            request.label = view.empty() ? shot.name : view;

            if (views.size() > 1) {
                request.group = prefix + SanitizeFileName(shot.name) + "_views";
                if (!sheets.contains(request.group)) {
                    sheets[request.group] = { .title = shot.name +
                            (prefix.empty() ? "" : " (" + job.variants[shotState.variant].name + ")"),
                        .path = job.outputDirectory / (request.group + ".png") };
                    sheetOrder.push_back(request.group);
                }
            }

            RequestCapture(request);

            // Variants of the same shot/view are compared in an additional sheet
            if (!job.variants.empty()) {
                auto group = baseName + "_variants";
                if (!sheets.contains(group)) {
                    sheets[group] = { .title = baseName + " variants", .path = job.outputDirectory / (group + ".png") };
                    sheetOrder.push_back(group);
                }
                variantCaptures[request.path.string()] = { group, job.variants[shotState.variant].name };
            }

            shotState.requested = true;
            return;
        }

        if (HasPendingCaptures())
            return;

        shotState.requested = false;
        shotState.settledFrames = 0;

        if (++shotState.view < views.size())
            return;
        shotState.view = 0;

        if (++shotState.shot < job.shots.size())
            return;
        shotState.shot = 0;

        if (++shotState.variant < variantCount)
            return;

        if (job.play)
            StartPlay();
        else
            phase = Phase::Finish;

    }

    void App::StartPlay() {

        playStartTime = std::chrono::steady_clock::now();
        events = job.timeline;

        for (auto it = events.begin(); it != events.end();) {
            if (it->when) {
                triggers.push_back({ .event = *it });
                it = events.erase(it);
            }
            else {
                ++it;
            }
        }

        // Strips are expanded into capture events
        for (const auto& strip : job.strips) {
            auto group = SanitizeFileName(strip.name);
            sheets[group] = {
                .title = strip.name, .columns = strip.columns, .path = job.outputDirectory / (group + ".png")
            };
            sheetOrder.push_back(group);

            if (strip.hasCamera)
                stripCameras[group] = strip.camera;

            // Integer steps avoid accumulating floating point errors
            auto count = int32_t(std::floor((strip.to - strip.from) / strip.every + 1e-3f));
            for (int32_t i = 0; i <= count; i++) {
                auto time = strip.from + float(i) * strip.every;

                EventSpec event;
                event.time = time;
                event.type = "capture";
                event.data["capture"] = json::object();
                event.data["capture"]["file"] = group + "/" + FormatFloat(time, 2) + "s.png";
                event.data["capture"]["group"] = group;
                event.data["capture"]["stripCamera"] = strip.hasCamera;
                events.push_back(event);
            }
        }

        std::stable_sort(events.begin(), events.end(), [](const EventSpec& a, const EventSpec& b) {

            return a.time < b.time;

        });

        auto beyond = std::remove_if(events.begin(), events.end(), [&](const EventSpec& event) {

            return event.time > job.duration + 1e-4f;

        });
        if (beyond != events.end()) {
            Warn(std::to_string(std::distance(beyond, events.end())) + " timeline events after the duration of " +
                FormatFloat(job.duration, 2) + "s are ignored");
            events.erase(beyond, events.end());
        }

        activeCamera = job.observer.value_or(CameraSpec {});

        // Focus cameras are framed once, re-framing a moving object every frame makes the camera jump
        if ((activeCamera->mode == "focus" || activeCamera->mode == "overview") && !activeCamera->dynamic) {
            ApplyCameraSpec(*activeCamera);
            const auto& camera = captureCameraEntity.GetComponent<CameraComponent>();

            CameraSpec fixed = *activeCamera;
            fixed.mode = "manual";
            fixed.position = camera.location;
            fixed.lookAt.reset();
            fixed.yaw = glm::degrees(camera.rotation.x);
            fixed.pitch = glm::degrees(camera.rotation.y);
            fixed.farPlane = camera.farPlane;
            activeCamera = fixed;
        }

        for (const auto& spec : job.scripts) {
            auto script = ResourceManager<Scripting::Script>::GetOrLoadResource(spec.path);
            if (!script.IsLoaded())
                throw std::runtime_error("Couldn't load script " + spec.path);

            std::vector<Scene::Entity> entities;
            if (spec.entity.empty()) {
                auto entity = scene->CreateEntity();
                entity.AddComponent<NameComponent>("Capture script " + fs::path(spec.path).stem().string());
                entity.AddComponent<TransformComponent>(mat4(1.0f), false);
                entities.push_back(entity);
            }
            else {
                entities = RemoveDescendants(*scene, FindEntities(*scene, spec.entity));
                if (entities.empty())
                    throw std::runtime_error("Script " + spec.path + ": no entity matches '" + spec.entity + "'");
            }

            for (auto entity : entities) {
                if (entity.HasComponent<LuaScriptComponent>())
                    throw std::runtime_error(
                        GetEntityLabel(entity) + " already has a script, attach to a new entity instead");
                entity.AddComponent<LuaScriptComponent>(script);
                Print("Attached script " + spec.path + " to " + GetEntityLabel(entity));
            }
        }

        // Enable play mode; scene.paused controls pauses and resetScripts temporarily leaves play mode
        scene->physicsWorld->pauseSimulation = false;
        playTime = 0.0f;
        nextEvent = 0;
        phase = Phase::Play;

        Print("Playing for " + FormatFloat(job.duration, 2) + "s with " + std::to_string(events.size()) +
            " timeline events and " + std::to_string(triggers.size()) + " conditional events");

    }

    void App::UpdatePlay() {

        if (stillState.active) {
            UpdateStill();
            return;
        }

        // A shown window plays in real time, the simulation still advances in fixed steps
        auto realPlayTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - playStartTime).count();
        if (job.showWindow && playTime > realPlayTime) {
            UpdateSceneWithCaptureCamera(*activeCamera, freezeDeltaTime);
            return;
        }

        for (auto it = pendingKeyReleases.begin(); it != pendingKeyReleases.end();) {
            if (it->first <= playTime + 1e-4f) {
                FireKey(it->second, false);
                it = pendingKeyReleases.erase(it);
            }
            else {
                ++it;
            }
        }

        UpdateTriggers();

        bool liveCapture = false;
        while (nextEvent < events.size() && events[nextEvent].time <= playTime + 1e-4f) {
            const auto& event = events[nextEvent];
            const auto& data = event.data;

            if (event.type == "capture") {
                const auto& capture = data["capture"];
                auto group = capture.value("group", std::string());

                CameraSpec camera = activeCamera.value_or(CameraSpec {});
                bool ownCamera = false;
                if (capture.contains("camera")) {
                    camera = ParseCamera(capture["camera"]);
                    ownCamera = true;
                }
                else if (capture.value("stripCamera", false)) {
                    camera = stripCameras[group];
                    ownCamera = true;
                }

                CaptureRequest request;
                auto file = capture.value("file",
                    SanitizeFileName(capture.value("name", "capture")) + "_" + FormatFloat(playTime, 2) + "s.png");
                request.path = job.outputDirectory / file;
                request.group = group;
                request.label = capture.value("label", FormatTime(event.time));
                request.time = event.time;
                if (capture.contains("passes"))
                    request.passes = capture["passes"].get<std::vector<std::string>>();

                // A different camera than the observer needs frozen frames to let temporal effects settle
                if (ownCamera || capture.value("still", false)) {
                    // Pending live captures are taken first, the still starts in the next frame
                    if (liveCapture)
                        break;

                    stillState = StillState { .active = true,
                        .camera = camera,
                        .request = request,
                        .settleFrames = capture.value("settleFrames", job.settleFrames) };
                    // The paused scene doesn't advance the exposure adaptation, it starts over for the new view
                    captureRenderer.InvalidateHistory();
                    nextEvent++;
                    UpdateStill();
                    return;
                }

                RequestCapture(request);
                liveCapture = true;
            }
            else if (event.type == "key") {
                const auto& key = data["key"];
                auto name = key.is_string() ? key.get<std::string>() : key.at("key").get<std::string>();
                auto action = key.is_object() ? key.value("action", std::string("press")) : std::string("press");
                if (action == "down" || action == "press")
                    FireKey(name, true);
                if (action == "up")
                    FireKey(name, false);
                if (action == "press") {
                    auto duration =
                        key.is_object() ? key.value("duration", job.fixedDeltaTime * 2.0f) : job.fixedDeltaTime * 2.0f;
                    pendingKeyReleases.emplace_back(playTime + duration, name);
                }
                Print(FormatTime(playTime) + " key " + name + " " + action);
            }
            else if (event.type == "teleport") {
                const auto& teleport = data["teleport"];
                auto position = ToVec3(teleport.at("position"));
                auto entities =
                    RemoveDescendants(*scene, FindEntities(*scene, teleport.at("entity").get<std::string>()));
                if (entities.empty())
                    Warn(FormatTime(playTime) + " teleport: no entity matches " + teleport["entity"].dump());

                for (auto entity : entities) {
                    if (auto player = entity.TryGetComponent<PlayerComponent>(); player && player->IsValid())
                        player->SetPosition(position);
                    SetWorldPosition(entity, position);
                    if (teleport.contains("yaw") || teleport.contains("pitch"))
                        SetLookDirection(entity, teleport);
                    Print(
                        FormatTime(playTime) + " teleported " + GetEntityLabel(entity) + " to " + FormatVec3(position));
                }
            }
            else if (event.type == "look") {
                const auto& look = data["look"];
                auto entities = RemoveDescendants(*scene, FindEntities(*scene, look.at("entity").get<std::string>()));
                if (entities.empty())
                    Warn(FormatTime(playTime) + " look: no entity matches " + look["entity"].dump());
                for (auto entity : entities)
                    SetLookDirection(entity, look);
            }
            else if (event.type == "resetScripts") {
                // Leaving play mode discards non-permanent script environments; scene.paused keeps their state
                scene->physicsWorld->pauseSimulation = true;
                UpdateSceneWithCaptureCamera(*activeCamera, freezeDeltaTime);
                scene->physicsWorld->pauseSimulation = false;
                Print(FormatTime(playTime) + " reset the state of all scripts");
            }
            else if (event.type == "pause") {
                // The job time continues, e.g. to check that nothing moves and scripts keep their state
                scene->paused = data["pause"].get<bool>();
                Print(FormatTime(playTime) + (scene->paused ? " paused the scene" : " resumed the scene"));
            }
            else if (event.type == "log") {
                Print(FormatTime(playTime) + " " + data["log"].get<std::string>());
            }
            else if (event.type == "expect") {
                RunExpectation(event);
            }

            nextEvent++;
        }

        // A live capture shows the regular step of this frame, which also gives correct motion vectors
        // The job only finishes once it was rendered
        if (!liveCapture && playTime >= job.duration - 1e-4f && nextEvent >= events.size()) {
            for (const auto& trigger : triggers) {
                if (trigger.fired)
                    continue;
                auto message = "Conditional event never happened: " + trigger.event.data.dump();
                if (trigger.event.optional)
                    Warn(message);
                else
                    checkRunner->AddFailure({ "timeline", trigger.event.type, playTime, message });
            }
            checkRunner->Run(*scene, playTime);
            if (tracker)
                tracker->Record(*scene, playTime);
            phase = Phase::Finish;
            return;
        }

        // The observer is the main camera during the update, culling, shadows and terrain LODs
        // are computed for the main camera. Scripts access their camera through their entity
        playerInput.Update(*scene, job.fixedDeltaTime);
        UpdateSceneWithCaptureCamera(*activeCamera);
        playTime += job.fixedDeltaTime;

        if (playTime + 1e-4f >= nextCheckTime) {
            checkRunner->Run(*scene, playTime);
            nextCheckTime += job.checks.interval;
        }
        if (tracker && playTime + 1e-4f >= nextTrackTime) {
            tracker->Record(*scene, playTime);
            nextTrackTime += tracker->spec.interval;
        }

    }

    void App::UpdateTriggers() {

        std::vector<EventSpec> due;
        for (auto& trigger : triggers) {
            const auto& when = *trigger.event.when;
            if (!trigger.metTime && playTime + 1e-4f >= trigger.event.time) {
                bool passed = trigger.logMatched;
                if (!when.contains("log"))
                    EvaluateExpectation(*scene, when, passed);
                if (passed) {
                    trigger.metTime = playTime;
                    Print(FormatTime(playTime) + " condition met: " + when.dump());
                }
            }

            if (trigger.metTime && !trigger.fired && playTime + 1e-4f >= *trigger.metTime + trigger.event.delay) {
                trigger.fired = true;
                auto event = trigger.event;
                event.time = playTime;
                due.push_back(event);
            }
        }

        // Due events run in this frame before the timed events of the frame
        events.insert(events.begin() + nextEvent, due.begin(), due.end());

    }

    void App::UpdateStill() {

        auto wasPaused = scene->paused;
        scene->paused = true;
        UpdateSceneWithCaptureCamera(stillState.camera, freezeDeltaTime);
        scene->paused = wasPaused;

        if (!stillState.requested) {
            if (stillState.settledFrames++ < stillState.settleFrames)
                return;

            RequestCapture(stillState.request);
            stillState.requested = true;
            return;
        }

        if (HasPendingCaptures())
            return;

        captureRenderer.InvalidateHistory();
        stillState = StillState {};

    }

    void App::WriteProfile() {

        auto threads = Graphics::Profiler::GetQueriesAverage(uint32_t(job.profileFrames));

        json queries = json::array();
        std::function<void(const Graphics::Profiler::Query&, const std::string&, int32_t)> addQuery =
            [&](const Graphics::Profiler::Query& query, const std::string& parent, int32_t depth) {

                auto path = parent.empty() ? query.name : parent + " / " + query.name;
                auto milliseconds = double(query.timer.elapsedTime) / 1000000.0;
                queries.push_back(
                    { { "query", path }, { "depth", depth }, { "ms", std::round(milliseconds * 10000.0) / 10000.0 } });
                for (const auto& child : query.children)
                    addQuery(child, path, depth + 1);

            };

        json profile;
        profile["frames"] = job.profileFrames;
        for (const auto& thread : threads) {
            queries = json::array();
            double total = 0.0;
            for (const auto& query : thread.queries) {
                total += double(query.timer.elapsedTime) / 1000000.0;
                addQuery(query, "", 0);
            }
            profile["threads"].push_back(
                { { "thread", thread.name }, { "ms", std::round(total * 10000.0) / 10000.0 }, { "queries", queries } });

            Print("GPU " + thread.name + ": " + FormatFloat(float(total), 3) + " ms per frame");
            for (const auto& query : queries)
                if (query["depth"].get<int32_t>() <= 1 && query["ms"].get<double>() >= 0.05)
                    Print(
                        "  " + FormatFloat(query["ms"].get<float>(), 3) + " ms  " + query["query"].get<std::string>());
        }

        WriteJson("profile.json", profile);

    }

    void App::WriteValidationReport() {

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
        report["validation"] = summary;

        Print("Validation: " + std::to_string(counts["error"]) + " errors, " + std::to_string(counts["warning"]) +
            " warnings, " + std::to_string(counts["performance"]) + " performance warnings (distinct ids)");
        for (const auto& [id, message] : sorted) {
            if (message.severity == "error")
                Print("VALIDATION " + id + " (" + std::to_string(message.count) + "x)");
        }

    }

    VkBool32 App::ValidationCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* data, void* userData) {

        auto app = static_cast<App*>(userData);
        auto id = data->pMessageIdName ? std::string(data->pMessageIdName) : std::to_string(data->messageIdNumber);

        std::lock_guard lock(app->validationMutex);
        auto& message = app->validationMessages[id];
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

    void App::Finish() {

        for (const auto& group : sheetOrder) {
            auto& sheet = sheets[group];
            if (sheet.images.empty())
                continue;

            auto image = ComposeSheet(sheet.images, sheet.labels, sheet.columns, sheet.title);
            if (WritePng(image, sheet.path))
                outputs.push_back(fs::relative(sheet.path, job.outputDirectory).generic_string());
        }

        if (tracker)
            WriteJson("trace.json", tracker->GetTrace());

        if (job.profileFrames > 0)
            WriteProfile();

        if (validationMessenger != VK_NULL_HANDLE)
            WriteValidationReport();

        if (fatal)
            jobExitCode = 2;
        else if ((checkRunner && checkRunner->HasFailures()) || expectationFailed)
            jobExitCode = 1;
        else
            jobExitCode = 0;

        std::string reason;
        if (fatal)
            reason = errors.empty() ? "unknown error" : errors.front();
        else if (jobExitCode == 1) {
            size_t failedExpectations =
                std::count_if(expectations.begin(), expectations.end(), [](const json& expectation) {

                    return !expectation["passed"].get<bool>();

                });
            auto failedChecks = checkRunner ? checkRunner->GetSummary().size() : size_t(0);
            reason = std::to_string(failedChecks) + " failed checks, " + std::to_string(failedExpectations) +
                " failed expectations";
        }

        report["job"] = job.name;
        report["runId"] = runId;
        report["scene"] = job.scene;
        report["status"] = jobExitCode == 0 ? "passed" : (jobExitCode == 1 ? "failed" : "error");
        report["exitCode"] = jobExitCode;
        if (!reason.empty())
            report["reason"] = reason;
        report["loadSeconds"] = loadSeconds;
        report["frames"] = frameCount;
        report["totalSeconds"] = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
        report["errors"] = errors;
        report["warnings"] = warnings;
        report["engineLogErrors"] = logErrorCount;
        if (checkRunner)
            report["checks"] = checkRunner->GetReport();
        if (!expectations.empty())
            report["expectations"] = expectations;
        if (job.play)
            report["playedSeconds"] = playTime;
        if (!triggers.empty()) {
            json triggerReport = json::array();
            for (const auto& trigger : triggers)
                triggerReport.push_back({ { "event", trigger.event.data }, { "fired", trigger.fired },
                    { "conditionMetAt", trigger.metTime ? json(*trigger.metTime) : json() } });
            report["conditionalEvents"] = triggerReport;
        }
        report["outputs"] = outputs;

        WriteJson("report.json", report);
        reportWritten = true;

        Print("---------------------------------------------");
        Print("Status: " + report["status"].get<std::string>() + (reason.empty() ? "" : " (" + reason + ")"));
        if (checkRunner) {
            for (const auto& line : checkRunner->GetSummary())
                Print("FAILED " + line);
        }
        for (const auto& expectation : expectations)
            if (!expectation["passed"].get<bool>())
                Print("FAILED expectation " + expectation.dump());
        for (const auto& error : errors)
            Print("ERROR " + error);
        Print("Outputs in " + job.outputDirectory.string() + ":");
        for (const auto& output : outputs)
            Print("  " + output.get<std::string>());

        phase = Phase::Done;
        ReleaseLock();
        Exit(jobExitCode);

    }

    void App::UpdateSceneWithCaptureCamera(const CameraSpec& spec, float deltaTime) {

        bool useCaptureCamera = UsesCaptureCamera(spec);
        if (useCaptureCamera)
            ApplyCameraSpec(spec);

        scene->Update(deltaTime < 0.0f ? job.fixedDeltaTime : deltaTime,
            useCaptureCamera ? captureCameraEntity : Scene::Entity());

    }

    bool App::UsesCaptureCamera(const CameraSpec& spec) const {

        if (spec.mode != "scene")
            return true;

        // Scenes without a main camera of their own are shown from an overview
        auto cameraSubset = scene->GetSubset<CameraComponent>();
        for (auto entity : cameraSubset) {
            if (entity != ECS::Entity(captureCameraEntity) && cameraSubset.Get(entity).isMain)
                return false;
        }

        return true;
    }

    void App::ApplyCameraSpec(const CameraSpec& spec) {

        auto& camera = captureCameraEntity.GetComponent<CameraComponent>();
        camera.aspectRatio = float(job.resolution.x) / float(job.resolution.y);
        camera.fieldOfView = spec.fieldOfView;
        camera.nearPlane = spec.nearPlane;
        camera.farPlane = spec.farPlane;

        vec3 position = camera.location;
        std::optional<vec3> target;
        float yaw = glm::radians(spec.yaw), pitch = glm::radians(spec.pitch);

        auto mode = spec.mode == "scene" ? std::string("overview") : spec.mode;

        if (mode == "manual") {
            position = spec.position;
            target = spec.lookAt;
        }
        else if (mode == "focus" || mode == "overview") {
            Volume::AABB bounds;
            float entityYaw = 0.0f;
            EntitySet focused;
            if (mode == "focus") {
                auto entities = RemoveDescendants(*scene, FindEntities(*scene, spec.entity));
                if (entities.empty()) {
                    // Entities created by scripts might not exist yet while loading or playing
                    if (phase == Phase::Play || phase == Phase::Loading)
                        return;
                    throw std::runtime_error("Camera focus: no entity matches '" + spec.entity + "'");
                }
                bounds = GetBounds(entities);
                entityYaw = glm::degrees(GetEntityYaw(entities.front()));
                for (auto entity : entities)
                    CollectHierarchy(entity, focused);
            }
            else {
                bounds = GetSceneBounds(*scene);
            }

            auto angles = GetViewAngles(spec.view.empty() ? (mode == "overview" ? "iso" : "front") : spec.view);
            auto azimuth = spec.azimuth.value_or(angles.azimuth) + (spec.local ? entityYaw : 0.0f);
            auto elevation = spec.elevation.value_or(angles.elevation);

            auto az = glm::radians(azimuth), el = glm::radians(elevation);
            auto direction = vec3(cos(el) * sin(az), sin(el), cos(el) * cos(az));

            auto center = bounds.GetCenter();
            auto radius = std::max(glm::length(bounds.GetSize()) * 0.5f, 0.25f);

            // Fit the bounding sphere into the smaller of the two field of view angles
            auto halfFov = glm::radians(spec.fieldOfView) * 0.5f;
            auto halfFovHorizontal = std::atan(std::tan(halfFov) * camera.aspectRatio);
            auto distance = spec.distance > 0.0f ?
                spec.distance :
                radius * spec.padding / std::sin(std::min(halfFov, halfFovHorizontal));

            // Prefer nearby unobstructed angles before reducing the camera distance
            if (mode == "focus" && spec.avoidOcclusion) {
                static const vec2 angleOffsets[] = { { 0.0f, 0.0f }, { 0.0f, 15.0f }, { 20.0f, 10.0f },
                    { -20.0f, 10.0f }, { 0.0f, 30.0f }, { 40.0f, 20.0f }, { -40.0f, 20.0f }, { 0.0f, 50.0f },
                    { 0.0f, -10.0f } };

                // Sample corners as well as the center to account for partial occlusion
                std::vector<vec3> samples = { center };
                for (auto corner : bounds.GetCorners()) {
                    samples.push_back(glm::mix(corner, center, 0.1f));
                }

                auto countVisible = [&](vec3 cameraPosition) {

                    int32_t visible = 0;
                    for (auto sample : samples) {
                        auto toSample = sample - cameraPosition;
                        auto length = glm::length(toSample);
                        if (length < 1e-4f ||
                            !Raycast(*scene, cameraPosition, toSample / length, length, focused).valid)
                            visible++;
                    }
                    return visible;

                };

                auto bestDirection = direction;
                auto bestVisible = 0;
                for (auto offset : angleOffsets) {
                    auto a = glm::radians(azimuth + offset.x);
                    auto e = glm::radians(glm::clamp(elevation + offset.y, -80.0f, 89.0f));
                    auto candidate = vec3(cos(e) * sin(a), sin(e), cos(e) * cos(a));

                    auto visible = countVisible(center + candidate * distance);
                    if (visible > bestVisible) {
                        bestVisible = visible;
                        bestDirection = candidate;
                    }
                    if (visible == int32_t(samples.size()))
                        break;
                }

                if (bestVisible > 0) {
                    direction = bestDirection;
                }
                else {
                    // Moving closer only helps if the camera stays outside of the focused bounds,
                    // otherwise the requested view is kept even though it's occluded
                    auto hit = Raycast(*scene, center, direction, distance, focused);
                    if (hit.valid && hit.distance - 0.25f > radius)
                        distance = hit.distance - 0.25f;
                }
            }

            position = center + direction * distance;
            target = center;
            camera.farPlane = std::max(spec.farPlane, distance + radius * 4.0f);
        }
        else if (mode == "follow") {
            auto entities = RemoveDescendants(*scene, FindEntities(*scene, spec.entity));
            if (entities.empty())
                return;

            auto bounds = GetEntityBounds(entities.front());
            auto center = bounds.GetCenter();
            auto offset = spec.offset;
            if (spec.local)
                offset = vec3(glm::rotate(GetEntityYaw(entities.front()), vec3(0.0f, 1.0f, 0.0f)) * vec4(offset, 0.0f));

            position = center + offset;
            target = center;
        }

        if (target && glm::distance(*target, position) > 1e-4f) {
            auto direction = glm::normalize(*target - position);
            yaw = std::atan2(direction.x, direction.z);
            pitch = std::asin(glm::clamp(direction.y, -1.0f, 1.0f));
        }

        camera.location = position;
        camera.rotation = vec2(yaw, pitch);

    }

    void App::RequestCapture(const CaptureRequest& request) {

        captureRequests.push_back(request);

    }

    bool App::HasPendingCaptures() const {

        return !captureRequests.empty() || !pendingReadbacks.empty();
    }

    void App::ProcessReadback() {

        if (pendingReadbacks.empty())
            return;

        auto requests = std::move(pendingReadbacks);
        pendingReadbacks.clear();

        // All requests of a frame were rendered into the same image
        auto image = captureRenderer.ReadOutput();
        std::map<std::string, Ref<Image>> passImages;

        for (const auto& request : requests) {
            if (!WritePng(image, request.path)) {
                Fail("Couldn't write " + request.path.string());
                continue;
            }

            auto relative = fs::relative(request.path, job.outputDirectory).generic_string();
            outputs.push_back(relative);
            Print("Captured " + relative + (phase == Phase::Play ? " at " + FormatTime(request.time) : ""));

            if (!request.group.empty())
                AddToSheet(request.group, image, request.label);

            if (auto it = variantCaptures.find(request.path.string()); it != variantCaptures.end())
                AddToSheet(it->second.first, image, it->second.second);

            if (request.passes.empty())
                continue;

            auto stem = request.path.parent_path() / request.path.stem();
            auto group = fs::relative(stem, job.outputDirectory).generic_string() + "_passes";
            sheets[group] = { .title = relative + " passes", .path = job.outputDirectory / (group + ".png") };
            sheetOrder.push_back(group);
            AddToSheet(group, image, "lit");

            for (const auto& pass : request.passes) {
                if (!passImages.contains(pass))
                    passImages[pass] = captureRenderer.ReadPass(pass);

                auto passPath = fs::path(stem.string() + "_" + pass + ".png");
                if (WritePng(passImages[pass], passPath))
                    outputs.push_back(fs::relative(passPath, job.outputDirectory).generic_string());
                AddToSheet(group, passImages[pass], pass);
            }
        }

    }

    void App::AddToSheet(const std::string& group, const Ref<Image>& image, const std::string& label) {

        auto& sheet = sheets[group];
        // Only the downscaled version is needed for the sheet
        sheet.images.push_back(Downscale(image, 640));
        sheet.labels.push_back(label);

    }

    void App::WriteJson(const std::string& fileName, const json& j) {

        auto path = job.outputDirectory / fileName;
        fs::create_directories(path.parent_path());

        if (!WriteFileAtomic(path, j.dump(2)))
            Warn("Couldn't write " + path.string());

        outputs.push_back(fileName);

    }

    void App::RunExpectation(const EventSpec& event) {

        bool passed = false;
        auto result = EvaluateExpectation(*scene, event.data["expect"], passed);
        result["t"] = playTime;
        result["passed"] = passed;
        expectations.push_back(result);

        if (!passed)
            expectationFailed = true;

        Print(FormatTime(playTime) + " expectation " + (passed ? "passed: " : "FAILED: ") + result.dump());

    }

    void App::SetLookDirection(Scene::Entity entity, const json& look) {

        auto camera = entity.TryGetComponent<CameraComponent>();
        if (!camera) {
            // Entities without a camera (e.g. driven by a script) are turned around the world Y axis instead
            auto transform = entity.TryGetComponent<TransformComponent>();
            if (!transform || !look.contains("yaw")) {
                Warn(GetEntityLabel(entity) + " has no camera, only the yaw of its transform can be set");
                return;
            }

            auto parentMatrix = transform->globalMatrix * glm::inverse(transform->matrix);
            auto global = glm::translate(vec3(transform->globalMatrix[3])) *
                glm::rotate(glm::radians(look["yaw"].get<float>()), vec3(0.0f, 1.0f, 0.0f));
            transform->Set(glm::inverse(parentMatrix) * global);
            transform->globalMatrix = global;
            Print(FormatTime(playTime) + " " + GetEntityLabel(entity) + " turned to yaw " +
                FormatFloat(look["yaw"].get<float>(), 1));
            return;
        }

        // Yaw 0 looks towards +Z, 90 towards +X
        if (look.contains("yaw"))
            camera->rotation.x = glm::radians(look["yaw"].get<float>());
        if (look.contains("pitch"))
            camera->rotation.y = glm::radians(look["pitch"].get<float>());

        Print(FormatTime(playTime) + " " + GetEntityLabel(entity) + " looks at yaw " +
            FormatFloat(glm::degrees(camera->rotation.x), 1) + ", pitch " +
            FormatFloat(glm::degrees(camera->rotation.y), 1));

    }

    void App::BuildOverlay() {

        overlay->Clear();
        if (!job.overlay)
            return;

        const auto& spec = *job.overlay;
        overlay->depthTest = spec.depthTest;

        for (auto entity : RemoveDescendants(*scene, FindEntities(*scene, spec.bounds)))
            overlay->Box(GetEntityBounds(entity), vec3(1.0f, 0.85f, 0.1f));

        if (spec.players) {
            auto playerSubset = scene->GetSubset<PlayerComponent>();
            for (auto entity : playerSubset)
                overlay->Figure(GetEntityPosition(entity), 1.8f, 0.35f, vec3(0.1f, 0.9f, 1.0f));
        }

        if (spec.trails && tracker) {
            for (const auto& [label, samples] : tracker->GetSamples())
                for (size_t i = 1; i < samples.size(); i++)
                    overlay->Line(samples[i - 1].position, samples[i].position, vec3(1.0f, 0.2f, 0.9f));
        }

        if (spec.queries) {
            for (const auto& result : queryResults) {
                const auto& query = result["query"];
                if (!result.contains("result"))
                    continue;
                const auto& hit = result["result"];
                auto type = query.value("type", std::string());

                if (type == "pick") {
                    for (const auto& pick : hit)
                        if (pick.value("hit", false))
                            overlay->Cross(ToVec3(pick["position"]), 0.3f, vec3(1.0f, 1.0f, 0.2f));
                    continue;
                }
                if (!hit.is_object())
                    continue;

                if (type == "raycast") {
                    auto origin = ToVec3(query["origin"]);
                    auto end = hit.value("hit", false) ?
                        ToVec3(hit["position"]) :
                        origin + glm::normalize(ToVec3(query["direction"])) * query.value("maxDistance", 4096.0f);
                    overlay->Line(
                        origin, end, hit.value("hit", false) ? vec3(0.2f, 1.0f, 0.2f) : vec3(1.0f, 0.2f, 0.2f));
                }
                if ((type == "raycast" || type == "ground") && hit.value("hit", false))
                    overlay->Cross(ToVec3(hit["position"]), 0.3f, vec3(0.2f, 1.0f, 0.2f));
                if (type == "path") {
                    const auto& samples = hit["samples"];
                    for (size_t i = 1; i < samples.size(); i++)
                        overlay->Line(ToVec3(samples[i - 1]["position"]) + vec3(0.0f, 0.05f, 0.0f),
                            ToVec3(samples[i]["position"]) + vec3(0.0f, 0.05f, 0.0f), vec3(0.2f, 1.0f, 0.2f));
                    for (const auto& issue : hit["issues"])
                        overlay->Cross(ToVec3(issue["at"]), 0.4f, vec3(1.0f, 0.15f, 0.15f));
                }
            }
        }

        for (const auto& marker : spec.markers)
            overlay->Cross(ToVec3(marker.at("position")), marker.value("size", 0.5f),
                ToVec3(marker.value("color", json()), vec3(1.0f, 0.3f, 0.1f)));

        for (const auto& axes : spec.axes)
            overlay->Axes(axes, 2.0f);

    }

    void App::FireKey(const std::string& key, bool down) {

        auto keyCode = SDL_GetKeyFromName(key.c_str());
        if (keyCode == SDLK_UNKNOWN)
            throw std::runtime_error("Unknown key '" + key + "', use SDL key names like E, Space, Left Shift, Up");

        SDL_KeyboardEvent event = {};
        event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
        event.state = down ? SDL_PRESSED : SDL_RELEASED;
        event.repeat = 0;
        event.keysym.sym = keyCode;
        event.keysym.scancode = SDL_GetScancodeFromKey(keyCode);

        Events::EventManager::KeyboardEventDelegate.Fire(Events::KeyboardEvent(event));

    }

}

Atlas::EngineInstance* GetEngineInstance() {

    return new Atlas::Capture::App();

}
