#include "App.h"
#include "TerrainTools.h"
#include "SceneEditor.h"

#include "Serializer.h"
#include "loader/ImageLoader.h"
#include "resource/ResourceManager.h"
#include "scripting/Script.h"
#include "scene/components/LuaScriptComponent.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/Profiler.h"
#include "audio/AudioManager.h"

#include <SDL3/SDL.h>

#include <cstdio>

const Atlas::EngineConfig Atlas::EngineInstance::engineConfig = {
    .assetDirectory = "../../data",
    .shaderDirectory = "shader"
};

namespace Atlas::Capture {

    using namespace Scene::Components;

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

    // Keep scripts active with a minimal time step while waiting for real-time playback
    static constexpr float freezeDeltaTime = 1e-5f;

    // Returns the exit code if the command line composes images instead of running a job
    static std::optional<int32_t> RunCompose(const std::vector<std::string>& args) {

        if (args.size() < 3 || args[1] != "--compose")
            return std::nullopt;

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
            auto label = separator == std::string::npos ? fs::path(args[i]).stem().string() : args[i].substr(0, separator);
            auto path = ResolveUserPath(separator == std::string::npos ? args[i] : args[i].substr(separator + 1));

            auto image = Loader::ImageLoader::LoadImage<uint8_t>(path.string(), false, 4);
            if (!image || !image->HasData()) {
                std::printf("Couldn't load %s\n", path.string().c_str());
                return 2;
            }
            images.push_back(image);
            labels.push_back(label);
        }

        auto sheet = ComposeSheet(images, labels, columns, title);
        auto written = WritePng(sheet, output);
        std::printf("[capture] Composed %zu images into %s\n", images.size(), output.string().c_str());
        return written ? 0 : 2;

    }

    void App::LoadContent() {

        startTime = std::chrono::steady_clock::now();
        report.InstallCrashHandlers();

        if (auto composeExitCode = RunCompose(args)) {
            phase = Phase::Done;
            Exit(*composeExitCode);
            return;
        }

        // A server or a pipeline runs several jobs, the other options of the command line apply to each of them
        std::string serve, pipelineFile, from;
        bool force = false;
        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "--serve" && i + 1 < args.size())
                serve = args[++i];
            else if (args[i] == "--pipeline" && i + 1 < args.size())
                pipelineFile = args[++i];
            else if (args[i] == "--from" && i + 1 < args.size())
                from = args[++i];
            else if (args[i] == "--force")
                force = true;
            else
                jobArguments.push_back(args[i]);
        }

        // Muted until a job asks for the sound
        Audio::AudioManager::Mute();

        try {
            if (!serve.empty()) {
                mode = Mode::Server;
                server.emplace(ResolveUserPath(serve));

                // Scenes can be loaded before the first job, which sets up the renderer for its resolution
                captureRenderer.Init(mainRenderer, job.resolution);
                defaultRenderScale = captureRenderer.GetRenderScale();
                rendererInitialized = true;
                overlay = std::make_unique<Overlay>();

                if (std::find(jobArguments.begin(), jobArguments.end(), "--hidden") == jobArguments.end()) {
                    font = CreateRef<Font>("font/roboto.ttf", 16.0f, 5);
                    auto panelHeight = CaptureRenderer::GetPanelHeight(font, outputLineCount + 1);
                    window.SetSize(job.resolution.x, job.resolution.y + int32_t(panelHeight));
                    window.Show();
                    SDL_RaiseWindow(window.GetSDLWindow());
                }

                report.Print("Serving commands from " + server->directory.string());
                phase = Phase::Idle;
                return;
            }
            if (!pipelineFile.empty()) {
                mode = Mode::Pipeline;
                pipeline = Pipeline::Load(ResolveUserPath(pipelineFile), from, force);
                report.Print("Pipeline '" + pipeline->name + "' with " + std::to_string(pipeline->steps.size()) + " steps, snapshots in " +
                    pipeline->snapshotDirectory.string());
                pipeline->Prepare([&](const std::string& message) { report.Print(message); });
                StartPipelineStep();
                return;
            }
            job = ParseCommandLine(args);
        }
        catch (const std::exception& e) {
            std::string message = e.what();
            std::printf("%s\n", message.c_str());
            phase = Phase::Done;
            Exit(message.starts_with("Usage:") ? 0 : 2);
            return;
        }

        StartJob(job);

    }

    Job App::ParseJobFile(const fs::path& path, const std::vector<std::string>& extraArguments) const {

        // Options before the job file are kept, the job file can't override them
        std::vector<std::string> jobArgs = { args.front() };
        jobArgs.insert(jobArgs.end(), jobArguments.begin(), jobArguments.end());
        jobArgs.insert(jobArgs.end(), extraArguments.begin(), extraArguments.end());
        jobArgs.push_back(path.string());
        return ParseCommandLine(jobArgs);

    }

    void App::ResetJobState() {

        fatal = false;
        activeCamera.reset();
        captureRequests.clear();
        pendingReadbacks.clear();
        sheets.clear();
        sheetOrder.clear();
        shotState = {};
        stillState = {};
        baseSettings = json();
        editFrames = 0;
        editState = EditState();
        variantCaptures.clear();
        stripCameras.clear();

        timeline.Reset(job.timeline);
        nextCheckTime = 0.0f;
        nextTrackTime = 0.0f;

        queryResults = json::array();
        checkRunner.reset();
        tracker.reset();

        report.Reset(job);

        frameCount = 0;
        readyFrame = -1;
        rtWaitFrames = 0;
        loadSeconds = 0.0f;

    }

    void App::StartJob(const Job& nextJob) {

        job = nextJob;
        ResetJobState();
        startTime = std::chrono::steady_clock::now();

        if (auto error = report.Begin(); !error.empty()) {
            report.Error(error);
            report.Print("Status: error (" + error + ")");
            report.exitCode = 2;
            report.data = { { "job", job.name }, { "status", "error" }, { "exitCode", 2 }, { "reason", error } };
            FinishJob();
            return;
        }

        if (job.showWindow) {
            if (!font)
                font = CreateRef<Font>("font/roboto.ttf", 16.0f, 5);

            // The frame keeps its aspect ratio above the output panel, large captures are shown smaller
            auto panelHeight = CaptureRenderer::GetPanelHeight(font, outputLineCount + 1);
            auto scale = std::min({ 1.0f, 1600.0f / float(job.resolution.x), 900.0f / float(job.resolution.y) });
            window.SetSize(int32_t(float(job.resolution.x) * scale), int32_t(float(job.resolution.y) * scale + panelHeight));
            window.Show();
            // Runs started from a background shell would otherwise open behind other windows
            SDL_RaiseWindow(window.GetSDLWindow());
        }
        else if (mode != Mode::Job) {
            window.Hide();
        }

        // The profiler is disabled by default in release builds
        Graphics::Profiler::enable = job.profileFrames > 0;

        if (job.sound)
            Audio::AudioManager::Unmute();
        else
            Audio::AudioManager::Mute();

        report.Print("Job '" + job.name + "' with scene " + job.scene + " (run " + report.runId + ")");

        if (job.validation && !report.EnableValidation()) {
            Fail("Validation layers aren't available, they are part of the Vulkan SDK");
            return;
        }
        report.Print("Output directory: " + job.outputDirectory.string());

        if (!rendererInitialized) {
            captureRenderer.Init(mainRenderer, job.resolution);
            defaultRenderScale = captureRenderer.GetRenderScale();
            rendererInitialized = true;
        }
        else if (captureRenderer.resolution != job.resolution) {
            // Resources of the previous resolution may still be in use by frames in flight
            graphicsDevice->WaitForIdle();
            captureRenderer.Resize(job.resolution);
        }
        captureRenderer.SetRenderScale(job.renderScale.value_or(defaultRenderScale));
        baseRenderScale = captureRenderer.GetRenderScale();
        overlay = std::make_unique<Overlay>();

        // The mouse over a shown window must not turn the players of a running job
        playerInput.useMouse = false;

        checkRunner = std::make_unique<CheckRunner>(job.checks);
        if (job.track)
            tracker = std::make_unique<Tracker>(*job.track);

        // Load with the camera of the first shot such that streaming and probes start in the right area. Entities an
        // edit adds don't exist yet, such shots are framed once the edit is done
        if (!job.shots.empty() && !(job.edit && job.shots.front().camera.mode == "focus"))
            activeCamera = job.shots.front().camera;
        else if (job.edit && !job.shots.empty())
            activeCamera = CameraSpec { .mode = "overview" };
        else
            activeCamera = job.observer.value_or(CameraSpec {});

        // Capture errors that happen while loading
        report.SkipLog();

        if (scene && scenePath == job.scene) {
            // The scene starts each job with the settings it was loaded with, out of play mode
            ApplySettings(*scene, sceneSettings);
            scene->physicsWorld->pauseSimulation = true;
            scene->paused = false;
            captureRenderer.InvalidateHistory();
            report.Print("Using the loaded scene");
            phase = Phase::Prepare;
            return;
        }
        if (scene) {
            Fail("The job uses " + job.scene + ", but " + scenePath + " is loaded, unload it first");
            return;
        }

        keepScene = false;
        LoadScene(job.scene);

    }

    void App::LoadScene(const std::string& path) {

        loadStartTime = std::chrono::steady_clock::now();
        frameCount = 0;
        rtWaitFrames = 0;
        scenePath = path;
        phase = Phase::Loading;

        try {
            auto binary = fs::path(path).extension() != ".json";
            scene = Serializer::DeserializeScene(path, binary);
        }
        catch (const std::exception& e) {
            scene.reset();
            Fail("Couldn't load scene " + path + ": " + e.what());
            return;
        }

        if (!scene) {
            Fail("Couldn't load scene " + path);
            return;
        }

        CreateCaptureCamera();

    }

    void App::UnloadScene() {

        if (!scene)
            return;

        graphicsDevice->WaitForIdle();
        scene.reset();
        scenePath.clear();
        sceneSettings = json();
        captureCameraEntity = Scene::Entity();
        keepScene = false;

    }

    void App::UnloadContent() {

        if (phase != Phase::Done && phase != Phase::Idle) {
            Fail("The window was closed before the job finished");
            Finish();
        }
        report.End();

        graphicsDevice->WaitForIdle();
        report.DestroyValidation();
        scene.reset();

    }

    void App::Update(float deltaTime) {

        if (phase == Phase::Done)
            return;

        report.BeginFrame(job.profileFrames);
        graphicsDevice->WaitForPreviousFrameSubmission();
        report.currentCpuTimes.wait = report.GetFrameMilliseconds();

        try {
            ProcessReadback();
            CollectLog();

            frameCount++;

            switch (phase) {
            case Phase::Idle:
                ProcessNextCommand();
                break;
            case Phase::Loading: {
                UpdateSceneWithCaptureCamera(*activeCamera);
                auto elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - loadStartTime).count();
                if (IsSceneReady()) {
                    for (const auto& mesh : scene->GetMeshes())
                        if (mesh.IsValid() && !mesh.IsLoaded())
                            report.Warn("Mesh " + mesh.GetResource()->path + " failed to load");

                    loadSeconds = elapsed;
                    sceneSettings = ExportSettings(*scene);
                    report.Print("Scene ready after " + FormatFloat(loadSeconds, 1) + "s (" + std::to_string(frameCount) +
                        " frames, " + std::to_string(scene->GetEntityCount() - 1) + " entities)");
                    retainedMeshes.clear();
                    if (loadingForCommand) {
                        loadingForCommand = false;
                        phase = Phase::Idle;
                        CompleteCommand({ { "status", "ok" }, { "loadSeconds", loadSeconds },
                            { "entities", scene->GetEntityCount() - 1 }, { "warnings", report.warnings } });
                    }
                    else {
                        phase = Phase::Prepare;
                    }
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

        // Only a scene updated in this frame is rendered, e.g. not while finishing, waiting for commands or in the frame a
        // job starts on a loaded scene. The renderer would use the per frame data of an older update, like an
        // acceleration structure that was never built because its frame wasn't rendered
        auto updated = sceneUpdated;
        sceneUpdated = false;
        if (!scene || !updated) {
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
            auto renderStart = std::chrono::steady_clock::now();
            BuildOverlay();
            captureRenderer.Render(scene, passes, overlay->GetBatches(), job.pathTrace);
            report.currentCpuTimes.render = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - renderStart).count();
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
        const auto& outputLines = report.outputLines;
        auto first = outputLines.size() > outputLineCount ? outputLines.size() - outputLineCount : 0;
        lines.insert(lines.end(), outputLines.begin() + first, outputLines.end());

        captureRenderer.Present(font, lines);

    }

    std::string App::GetStatusLine() const {

        if (phase == Phase::Idle)
            return "waiting for commands  |  " + (scene ? scenePath + " loaded" : std::string("no scene loaded"));

        std::string line = job.name + "  |  ";
        switch (phase) {
        case Phase::Idle:
            break;
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
            line += "playing  " + FormatFloat(timeline.time, 2) + " / " + FormatFloat(job.duration, 2) + " s";
            if (scene && scene->paused)
                line += "  (paused)";
            break;
        case Phase::Finish:
            line += "writing outputs";
            break;
        case Phase::Done:
            line += report.exitCode == 0 ? "passed" : report.exitCode == 1 ? "failed" : "error";
            break;
        }

        return line + "  |  frame " + std::to_string(frameCount);

    }

    void App::Fail(const std::string& message) {

        report.Error(message);
        fatal = true;

        // A scene loaded by a command has no job and no report
        if (loadingForCommand) {
            loadingForCommand = false;
            retainedMeshes.clear();
            UnloadScene();
            phase = Phase::Idle;
            CompleteCommand({ { "status", "error" }, { "reason", message } });
            return;
        }

        if (phase != Phase::Done)
            phase = Phase::Finish;

    }

    void App::CollectLog() {

        for (const auto& entry : report.CollectLog()) {
            timeline.MatchLog(entry.message);
            if (entry.type == Log::TYPE_ERROR && checkRunner && job.checks.logErrors && !MatchAnyPattern(job.checks.logIgnore, entry.message))
                checkRunner->AddFailure({ "log", entry.message.substr(0, 160), timeline.time, entry.message });
        }

    }

    bool App::IsSceneReady() {

        // A few frames are needed to kick off the asynchronous work
        if (frameCount < 3)
            return false;

        // Meshes that failed to load count as loaded, they're reported once the scene is ready
        if (!scene->IsFullyLoaded())
            return false;

        // Ray tracing structures are built asynchronously, but not every device/scene builds them
        if (scene->IsRtDataNeeded() && !scene->IsRtDataValid() && rtWaitFrames++ < 300)
            return false;

        return true;

    }

    void App::Prepare() {

        // The renderer expects a scene update in every frame
        UpdateSceneWithCaptureCamera(*activeCamera);

        if (!job.settings.empty())
            ApplySettings(*scene, job.settings);
        baseSettings = ExportSettings(*scene);
        report.Print("Rendering at " + std::to_string(job.resolution.x) + "x" + std::to_string(job.resolution.y) + GetRenderDescription());

        EntitySet excluded = { captureCameraEntity };

        if (job.inspect) {
            auto inspection = InspectScene(*scene, job.inspect->filter, job.inspect->detail, excluded);
            report.WriteJson(job.inspect->file, inspection);
            report.Print("Inspection: " + std::to_string(inspection["entities"].size()) + " entities written to " + job.inspect->file);
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

            auto results = RunQueries(*scene, job.queries, pickRay, job.outputDirectory);
            report.WriteJson("queries.json", results);
            queryResults = results;
            for (const auto& result : results) {
                auto text = result.contains("error") ? "error: " + result["error"].get<std::string>() : result["result"].dump();
                if (text.size() > 400)
                    text = text.substr(0, 400) + "... (see queries.json)";
                report.Print("Query " + result["query"].dump() + " -> " + text);
            }
        }

        if (job.edit) {
            report.data["edit"]["before"] = InspectScene(*scene, {}, {}, excluded);
            editState.loadedTerrain = scene->terrain.IsValid() ? scene->terrain.GetResource()->path : "";
            editState.dryRun = job.edit->dryRun;
            editState.textureFormat = job.edit->textureFormat == "ktx2" ? Loader::ImportTextureFormat::KTX2 :
                Loader::ImportTextureFormat::Original;
            scene->WaitForAsyncWorkCompletion();
            report.data["edit"]["operations"] = ApplyEditOperations(scene, job.edit->operations, editState);
            report.Print("Applied " + std::to_string(job.edit->operations.size()) + " edit operations");
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
        auto& edit = report.data["edit"];

        auto after = InspectScene(*scene, {}, {}, excluded);
        auto diff = DiffInspections(edit["before"], after);
        edit.erase("before");
        report.WriteJson("edit_diff.json", diff);
        edit["diff"] = diff["summary"];
        report.Print("Edit diff: " + diff["summary"].get<std::string>() + " (see edit_diff.json)");

        // Queries of the edited scene, e.g. to verify placements
        if (!job.edit->queries.empty()) {
            auto results = RunQueries(*scene, job.edit->queries, {}, job.outputDirectory);
            report.WriteJson("edit_queries.json", results);
            for (const auto& result : results) {
                auto text = result.contains("error") ? "error: " + result["error"].get<std::string>() : result["result"].dump();
                if (text.size() > 400)
                    text = text.substr(0, 400) + "... (see edit_queries.json)";
                report.Print("Edit query " + result["query"].value("type", "") + " -> " + text);
            }
        }

        checkRunner->Run(*scene, 0.0f);
        if (checkRunner->HasFailures()) {
            edit["saved"] = false;
            Fail("Checks failed after the edit, neither the scene nor materials were saved");
            return;
        }

        if (job.edit->dryRun) {
            edit["saved"] = false;
            report.Print("Dry run, neither the scene nor materials are saved");
            phase = job.shots.empty() ? Phase::Finish : Phase::Shots;
            return;
        }

        // Edits which only changed materials leave the scene untouched, no need to rewrite it
        bool sceneChanged = !diff["changed"].empty() || !diff["added"].empty() || !diff["removed"].empty() || job.edit->saveUnchanged ||
            editState.terrainChanged;

        auto output = job.edit->output;
        if (output.empty()) {
            auto scenePath = fs::path(job.scene);
            auto fileName = scenePath.stem().string() + ".edit" + scenePath.extension().string();
            output = (scenePath.parent_path() / fileName).generic_string();
        }

        // Validate the output first, materials are only written together with the scene
        auto absoluteOutput = GetAssetDirectory() / output;
        if (sceneChanged && output == job.scene && !job.edit->overwrite) {
            Fail("Refusing to overwrite the input scene " + output + ", set edit.overwrite = true or choose another output");
            return;
        }
        if (sceneChanged && fs::exists(absoluteOutput) && output != job.scene && !job.edit->overwrite) {
            Fail("Output scene " + output + " already exists, set edit.overwrite = true to replace it");
            return;
        }

        if (!editState.changedMaterials.empty()) {
            edit["materials"] = SaveMaterials(editState.changedMaterials);
            report.Print("Saved " + std::to_string(editState.changedMaterials.size()) + " edited materials");
        }

        if (!sceneChanged) {
            edit["saved"] = false;
            report.Print("The scene itself didn't change, it is not saved");
            phase = job.shots.empty() ? Phase::Finish : Phase::Shots;
            return;
        }

        // The snapshots of a pipeline keep the previous states
        if (fs::exists(absoluteOutput) && mode != Mode::Pipeline) {
            auto timestamp = std::chrono::system_clock::now().time_since_epoch() / std::chrono::seconds(1);
            auto backup = absoluteOutput;
            backup += ".backup-" + std::to_string(timestamp);
            fs::copy_file(absoluteOutput, backup);
            edit["backup"] = backup.string();
            report.Print("Backup of the previous scene: " + backup.string());
        }

        // The capture camera must not end up in the scene, the editor does the same with its camera
        auto entityCount = scene->GetEntityCount() - 1;
        scene->DestroyEntity(captureCameraEntity);

        if (editState.terrainChanged) {
            SaveSceneTerrain(*scene);
            edit["terrain"] = scene->terrain.GetResource()->path;
            report.Print("Saved terrain to data/" + scene->terrain.GetResource()->path);
        }

        auto binary = fs::path(output).extension() != ".json";
        Serializer::SerializeScene(scene, output, false, binary);

        CreateCaptureCamera();

        // A pipeline step that continues with the scene saves it again, only the last saved state is loaded again
        if (mode != Mode::Pipeline || !StepReadsScene(pipeline->currentStep + 1, output)) {
            try {
                auto reloaded = Serializer::DeserializeScene(output, binary);
                auto reloadedCount = reloaded->GetEntityCount();
                edit["verified"] = reloadedCount == entityCount;
                if (reloadedCount != entityCount)
                    Fail("Saved scene has " + std::to_string(reloadedCount) + " entities, expected " + std::to_string(entityCount));
            }
            catch (const std::exception& e) {
                Fail("Saved scene " + output + " couldn't be loaded again: " + e.what());
                return;
            }
        }

        edit["saved"] = true;
        edit["output"] = output;
        report.Print("Saved edited scene to data/" + output);

        // A scene that stays loaded starts the next job with the settings it was saved with
        sceneSettings = ExportSettings(*scene);

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
            captureRenderer.SetRenderScale(variant.renderScale.value_or(baseRenderScale));
            report.Print("Variant '" + variant.name + "'" + GetRenderDescription());
        }

        auto camera = shot.camera;
        if (!view.empty()) {
            camera.view = view;
            camera.azimuth.reset();
            camera.elevation.reset();
            if (camera.mode == "scene" || camera.mode == "manual")
                throw std::runtime_error("Shot '" + shot.name + "' has views, which require a focus or overview camera");
        }
        activeCamera = camera;

        UpdateSceneWithCaptureCamera(camera);

        auto settleFrames = shot.settleFrames >= 0 ? shot.settleFrames : job.settleFrames;
        if (!shotState.requested) {
            if (shotState.settledFrames++ < settleFrames)
                return;

            auto variantName = job.variants.empty() ? std::string() : job.variants[shotState.variant].name;
            auto prefix = job.variants.empty() ? std::string() : SanitizeFileName(variantName) + "/";
            auto baseName = SanitizeFileName(shot.name) + (view.empty() ? "" : "_" + view);

            CaptureRequest request;
            request.path = job.outputDirectory / (prefix + baseName + ".png");
            request.passes = shot.passes;
            request.label = view.empty() ? shot.name : view;

            if (views.size() > 1) {
                request.group = prefix + SanitizeFileName(shot.name) + "_views";
                if (!sheets.contains(request.group)) {
                    auto title = shot.name + (job.variants.empty() ? "" : " (" + variantName + ")");
                    sheets[request.group] = { .title = title, .path = job.outputDirectory / (request.group + ".png") };
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
                variantCaptures[request.path.string()] = { group, variantName };
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

        // The scripts get new entities, the asynchronous work of the last scene update (e.g. the ray tracing world)
        // still reads the entity storage
        scene->WaitForAsyncWorkCompletion();

        playStartTime = std::chrono::steady_clock::now();
        auto events = job.timeline;

        // Strips are expanded into capture events
        for (const auto& strip : job.strips) {
            auto group = SanitizeFileName(strip.name);
            sheets[group] = { .title = strip.name, .columns = strip.columns, .path = job.outputDirectory / (group + ".png") };
            sheetOrder.push_back(group);

            if (strip.hasCamera)
                stripCameras[group] = strip.camera;

            // Integer steps avoid accumulating floating point errors
            auto count = int32_t(std::floor((strip.to - strip.from) / strip.every + 1e-3f));
            for (int32_t i = 0; i <= count; i++) {
                auto time = strip.from + float(i) * strip.every;

                CaptureEvent capture;
                capture.file = group + "/" + FormatFloat(time, 2) + "s.png";
                capture.group = group;
                capture.stripCamera = strip.hasCamera;
                events.push_back({ .time = time, .type = "capture", .action = capture });
            }
        }

        if (auto dropped = timeline.Start(std::move(events), job.duration))
            report.Warn(std::to_string(dropped) + " timeline events after the duration of " + FormatFloat(job.duration, 2) + "s are ignored");

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
                entities = FindTopmostEntities(*scene, spec.entity);
                if (entities.empty())
                    throw std::runtime_error("Script " + spec.path + ": no entity matches '" + spec.entity + "'");
            }

            for (auto entity : entities) {
                if (entity.HasComponent<LuaScriptComponent>())
                    throw std::runtime_error(GetEntityLabel(entity) + " already has a script, attach to a new entity instead");
                entity.AddComponent<LuaScriptComponent>(script);
                report.Print("Attached script " + spec.path + " to " + GetEntityLabel(entity));
            }
        }

        // Enable play mode; scene.paused controls pauses and resetScripts temporarily leaves play mode
        scene->physicsWorld->pauseSimulation = false;
        phase = Phase::Play;

        report.Print("Playing for " + FormatFloat(job.duration, 2) + "s with " + std::to_string(timeline.GetTimedEventCount()) +
            " timeline events and " + std::to_string(timeline.GetTriggerCount()) + " conditional events");

    }

    void App::UpdatePlay() {

        if (stillState.active) {
            UpdateStill();
            return;
        }

        // A shown window plays in real time, the simulation still advances in fixed steps
        auto realPlayTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - playStartTime).count();
        if (job.showWindow && job.realTime && timeline.time > realPlayTime) {
            UpdateSceneWithCaptureCamera(*activeCamera, freezeDeltaTime);
            return;
        }

        timeline.Update(*scene, [&](const std::string& message) { report.Print(message); });

        bool liveCapture = false;
        while (auto due = timeline.GetDueEvent()) {
            const auto& event = *due;

            if (auto capture = std::get_if<CaptureEvent>(&event.action)) {
                CameraSpec camera = activeCamera.value_or(CameraSpec {});
                bool ownCamera = false;
                if (capture->camera) {
                    camera = *capture->camera;
                    ownCamera = true;
                }
                else if (capture->stripCamera) {
                    camera = stripCameras[capture->group];
                    ownCamera = true;
                }

                CaptureRequest request;
                auto defaultFile = SanitizeFileName(capture->name) + "_" + FormatFloat(timeline.time, 2) + "s.png";
                request.path = job.outputDirectory / capture->file.value_or(defaultFile);
                request.group = capture->group;
                request.label = capture->label.value_or(FormatTime(event.time));
                request.time = event.time;
                request.passes = capture->passes;

                // A different camera than the observer needs frozen frames to let temporal effects settle
                if (ownCamera || capture->still) {
                    // Pending live captures are taken first, the still starts in the next frame
                    if (liveCapture)
                        break;

                    stillState = StillState {
                        .active = true,
                        .camera = camera,
                        .request = request,
                        .settleFrames = capture->settleFrames.value_or(job.settleFrames)
                    };
                    // The paused scene doesn't advance the exposure adaptation, it starts over for the new view
                    captureRenderer.InvalidateHistory();
                    timeline.PopEvent();
                    UpdateStill();
                    return;
                }

                RequestCapture(request);
                liveCapture = true;
            }
            else {
                std::visit([&](const auto& action) {
                    if constexpr (!std::is_same_v<std::decay_t<decltype(action)>, CaptureEvent>)
                        RunEvent(action);
                }, event.action);
            }

            timeline.PopEvent();
        }

        // A live capture shows the regular step of this frame, which also gives correct motion vectors
        // The job only finishes once it was rendered
        if (!liveCapture && timeline.IsFinished(job.duration)) {
            for (const auto& event : timeline.GetMissedEvents()) {
                auto message = "Conditional event never happened: " + event.data.dump();
                if (event.optional)
                    report.Warn(message);
                else
                    checkRunner->AddFailure({ "timeline", event.type, timeline.time, message });
            }
            checkRunner->Run(*scene, timeline.time);
            if (tracker)
                tracker->Record(*scene, timeline.time);
            phase = Phase::Finish;
            return;
        }

        // The observer is the main camera during the update, culling, shadows and terrain LODs
        // are computed for the main camera. Scripts access their camera through their entity
        playerInput.Update(*scene, job.fixedDeltaTime);
        UpdateSceneWithCaptureCamera(*activeCamera);
        timeline.time += job.fixedDeltaTime;

        if (timeline.IsDue(nextCheckTime)) {
            checkRunner->Run(*scene, timeline.time);
            nextCheckTime += job.checks.interval;
        }
        if (tracker && timeline.IsDue(nextTrackTime)) {
            tracker->Record(*scene, timeline.time);
            nextTrackTime += tracker->spec.interval;
        }

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

    void App::Finish() {

        for (const auto& group : sheetOrder) {
            auto& sheet = sheets[group];
            if (sheet.images.empty())
                continue;

            auto image = ComposeSheet(sheet.images, sheet.labels, sheet.columns, sheet.title);
            if (WritePng(image, sheet.path))
                report.AddOutput(fs::relative(sheet.path, job.outputDirectory).generic_string());
        }

        if (tracker)
            report.WriteJson("trace.json", tracker->GetTrace());

        if (job.profileFrames > 0)
            report.WriteProfile(job.profileFrames);

        if (report.IsValidationEnabled())
            report.WriteValidation();

        auto failedExpectations = std::count_if(report.expectations.begin(), report.expectations.end(), [](const json& expectation) {

            return !expectation["passed"].get<bool>();

        });

        if (fatal)
            report.exitCode = 2;
        else if ((checkRunner && checkRunner->HasFailures()) || failedExpectations > 0)
            report.exitCode = 1;
        else
            report.exitCode = 0;

        std::string reason;
        if (fatal) {
            reason = report.errors.empty() ? "unknown error" : report.errors.front();
        }
        else if (report.exitCode == 1) {
            auto failedChecks = checkRunner ? checkRunner->GetSummary().size() : size_t(0);
            reason = std::to_string(failedChecks) + " failed checks, " + std::to_string(failedExpectations) + " failed expectations";
        }

        std::string status = report.exitCode == 0 ? "passed" : (report.exitCode == 1 ? "failed" : "error");
        json result = {
            { "job", job.name }, { "runId", report.runId }, { "scene", job.scene }, { "status", status },
            { "exitCode", report.exitCode }, { "renderScale", baseRenderScale }, { "loadSeconds", loadSeconds },
            { "frames", frameCount }, { "errors", report.errors }, { "warnings", report.warnings },
            { "totalSeconds", std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count() },
            { "engineLogErrors", report.logErrorCount }
        };
        if (!reason.empty())
            result["reason"] = reason;
        if (checkRunner)
            result["checks"] = checkRunner->GetReport();
        if (!report.expectations.empty())
            result["expectations"] = report.expectations;
        if (job.play)
            result["playedSeconds"] = timeline.time;
        if (auto triggers = timeline.GetTriggerReport(); !triggers.is_null())
            result["conditionalEvents"] = triggers;
        report.Write(result);

        report.Print("---------------------------------------------");
        report.Print("Status: " + status + (reason.empty() ? "" : " (" + reason + ")"));
        if (checkRunner) {
            for (const auto& line : checkRunner->GetSummary())
                report.Print("FAILED " + line);
        }
        for (const auto& expectation : report.expectations)
            if (!expectation["passed"].get<bool>())
                report.Print("FAILED expectation " + expectation.dump());
        for (const auto& error : report.errors)
            report.Print("ERROR " + error);
        report.Print("Outputs in " + job.outputDirectory.string() + ":");
        for (const auto& output : report.GetOutputs())
            report.Print("  " + output.get<std::string>());

        FinishJob();

    }

    void App::FinishJob() {

        report.End();

        if (mode == Mode::Job) {
            phase = Phase::Done;
            Exit(report.exitCode);
            return;
        }

        if (mode == Mode::Server) {
            // A scene loaded for this job only is unloaded again
            if (!keepScene)
                UnloadScene();
            phase = Phase::Idle;
            CompleteCommand({ { "status", report.data.value("status", "error") }, { "exitCode", report.exitCode },
                { "reason", report.data.value("reason", "") }, { "outputDirectory", job.outputDirectory.string() },
                { "report", (job.outputDirectory / "report.json").string() } });
            return;
        }

        if (report.exitCode != 0)
            pipeline->FailStep(report.exitCode);
        else
            pipeline->CompleteStep(report.data);

        const auto& edit = report.data.value("edit", json::object());
        auto savedScene = edit.value("saved", false) ? edit["output"].get<std::string>() : "";
        if (!savedScene.empty() && !job.play && StepReadsScene(pipeline->currentStep, savedScene)) {
            scenePath = savedScene;
            keepScene = true;
        }
        else {
            UnloadScene();
        }

        StartPipelineStep();

    }

    bool App::StepReadsScene(size_t step, const std::string& scene) {

        if (step >= pipeline->steps.size())
            return false;

        try {
            return ParseJobFile(pipeline->steps[step].jobFile).scene == scene;
        }
        catch (const std::exception&) {
            // Reported when the step starts
            return false;
        }

    }

    void App::StartPipelineStep() {

        if (pipeline->IsFinished()) {
            auto summaryPath = pipeline->WriteSummary();

            report.Print("---------------------------------------------");
            for (const auto& step : pipeline->GetSummary())
                report.Print("Step " + step["step"].get<std::string>() + ": " + step["result"].get<std::string>());
            report.Print(std::string("Pipeline ") + (pipeline->exitCode == 0 ? "passed" : "failed") + ", summary in " + summaryPath.string());

            UnloadScene();
            phase = Phase::Done;
            Exit(pipeline->exitCode);
            return;
        }

        const auto& step = pipeline->steps[pipeline->currentStep];
        report.Print("Pipeline step " + std::to_string(pipeline->currentStep + 1) + " / " + std::to_string(pipeline->steps.size()) + ": " + step.name);

        Job stepJob;
        try {
            stepJob = ParseJobFile(step.jobFile);
        }
        catch (const std::exception& e) {
            report.Print(std::string("ERROR: ") + e.what());
            pipeline->FailStep(2);
            StartPipelineStep();
            return;
        }

        StartJob(stepJob);

    }

    void App::ProcessNextCommand() {

        auto command = server->Poll();
        if (!command)
            return;

        if (command->is_discarded() || !command->is_object()) {
            CompleteCommand({ { "status", "error" }, { "reason", "The command isn't a JSON object" } });
            return;
        }

        static const std::map<std::string, void (App::*)(const json&)> commands = {
            { "loadScene", &App::LoadSceneCommand },
            { "reloadScene", &App::ReloadSceneCommand },
            { "unloadScene", &App::UnloadSceneCommand },
            { "run", &App::RunCommand },
            { "stop", &App::StopCommand }
        };

        try {
            auto type = command->value("command", std::string(command->contains("job") ? "run" : ""));
            report.Print("Command '" + server->commandName + "': " + type);

            auto handler = commands.find(type);
            if (handler == commands.end()) {
                std::string known;
                for (const auto& [name, _] : commands)
                    known += (known.empty() ? "" : ", ") + name;
                throw std::runtime_error("Unknown command '" + type + "' (" + known + ")");
            }
            (this->*handler->second)(*command);
        }
        catch (const std::exception& e) {
            report.Print(std::string("ERROR: ") + e.what());
            CompleteCommand({ { "status", "error" }, { "reason", e.what() } });
        }

    }

    void App::LoadSceneCommand(const json& command) {

        LoadSceneForCommand(ToAssetPath(command.at("scene").get<std::string>()), command);

    }

    void App::ReloadSceneCommand(const json& command) {

        if (scenePath.empty())
            throw std::runtime_error("No scene is loaded");

        // The meshes with their materials and textures stay loaded until the scene is ready again
        if (scene)
            retainedMeshes = scene->GetMeshes();
        LoadSceneForCommand(scenePath, command);

    }

    void App::LoadSceneForCommand(std::string path, const json& command) {

        UnloadScene();

        // A minimal job, its timeout and resolution apply while loading
        job = Job();
        job.name = server->commandName;
        job.scene = path;
        job.loadTimeout = command.value("timeout", job.loadTimeout);
        ResetJobState();
        activeCamera = command.contains("camera") ? ParseCamera(command["camera"]) : CameraSpec {};
        report.SkipLog();

        keepScene = true;
        loadingForCommand = true;
        report.Print("Loading " + path);
        LoadScene(path);

    }

    void App::UnloadSceneCommand(const json&) {

        UnloadScene();
        CompleteCommand({ { "status", "ok" } });

    }

    void App::RunCommand(const json& command) {

        fs::path jobFile;
        const auto& jobJson = command.at("job");
        if (jobJson.is_string()) {
            jobFile = ResolveUserPath(jobJson.get<std::string>());
        }
        else {
            // Inline jobs are written next to the processed command
            auto inlineJob = jobJson;
            if (!inlineJob.contains("name"))
                inlineJob["name"] = server->commandName;
            if (!inlineJob.contains("scene") && scene)
                inlineJob["scene"] = scenePath;
            jobFile = server->directory / "done" / (server->commandName + ".job.json");
            std::ofstream stream(jobFile);
            stream << inlineJob.dump(2);
        }
        StartJob(ParseJobFile(jobFile, command.value("args", std::vector<std::string>())));

    }

    void App::StopCommand(const json&) {

        CompleteCommand({ { "status", "ok" } });
        UnloadScene();
        phase = Phase::Done;
        Exit(0);

    }

    void App::CompleteCommand(const json& result) {

        server->Complete(result, scene ? scenePath : "");
        report.Print("Command '" + server->commandName + "' done: " + result.value("status", ""));

    }

    std::string App::GetRenderDescription() const {

        const auto& postProcessing = scene->postProcessing;
        auto upscaler = postProcessing.fsr2 ? "FSR2" : (postProcessing.taa.enable ? "TAA" : "no upscaling");
        return ", render scale " + FormatFloat(captureRenderer.GetRenderScale(), 2) + " (" + upscaler + ")";

    }

    void App::CreateCaptureCamera() {

        captureCameraEntity = scene->CreateEntity();
        auto& camera = captureCameraEntity.AddComponent<CameraComponent>(60.0f, float(job.resolution.x) / float(job.resolution.y), 0.1f, 2000.0f);
        camera.isMain = false;

    }

    void App::UpdateSceneWithCaptureCamera(const CameraSpec& spec, float deltaTime) {

        bool useCaptureCamera = UsesCaptureCamera(spec);
        if (useCaptureCamera)
            ApplyCameraSpec(spec);

        // Path tracing needs the ray tracing data, which the scene only builds when an effect uses it
        if (scene->rayTracingWorld)
            scene->rayTracingWorld->includeObjectHistory = job.pathTrace;

        auto updateStart = std::chrono::steady_clock::now();
        scene->Update(deltaTime < 0.0f ? job.fixedDeltaTime : deltaTime, useCaptureCamera ? captureCameraEntity : Scene::Entity());
        sceneUpdated = true;
        report.currentCpuTimes.update += std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - updateStart).count();

    }

    bool App::UsesCaptureCamera(const CameraSpec& spec) const {

        if (spec.mode != "scene")
            return true;

        // Scenes without a main camera of their own are shown from an overview
        auto cameraSubset = scene->GetSubset<CameraComponent>();
        for (auto entity : cameraSubset)
            if (entity != ECS::Entity(captureCameraEntity) && cameraSubset.Get(entity).isMain)
                return false;

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
            if (spec.groundRelative) {
                auto top = GetSceneBounds(*scene).max.y + 10.0f;
                auto groundHeight = [&](vec3 point) {

                    auto ground = GroundBelow(*scene, vec3(point.x, top, point.z), { captureCameraEntity });
                    return ground.valid ? ground.position.y : 0.0f;

                };
                position.y += groundHeight(position);
                if (target)
                    target->y += groundHeight(*target);
            }
        }
        else if (mode == "focus" || mode == "overview") {
            Volume::AABB bounds;
            float entityYaw = 0.0f;
            EntitySet focused;
            if (mode == "focus") {
                auto entities = FindTopmostEntities(*scene, spec.entity);
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

            auto direction = GetDirection(azimuth, elevation);

            auto center = bounds.GetCenter();
            auto radius = std::max(glm::length(bounds.GetSize()) * 0.5f, 0.25f);

            // Fit the bounding sphere into the smaller of the two field of view angles
            auto halfFov = glm::radians(spec.fieldOfView) * 0.5f;
            auto halfFovHorizontal = std::atan(std::tan(halfFov) * camera.aspectRatio);
            auto distance = spec.distance > 0.0f ? spec.distance : radius * spec.padding / std::sin(std::min(halfFov, halfFovHorizontal));

            // Prefer nearby unobstructed angles before reducing the camera distance
            if (mode == "focus" && spec.avoidOcclusion) {
                static const vec2 angleOffsets[] = { { 0.0f, 0.0f }, { 0.0f, 15.0f }, { 20.0f, 10.0f }, { -20.0f, 10.0f },
                    { 0.0f, 30.0f }, { 40.0f, 20.0f }, { -40.0f, 20.0f }, { 0.0f, 50.0f }, { 0.0f, -10.0f } };

                // Sample corners as well as the center to account for partial occlusion
                std::vector<vec3> samples = { center };
                for (auto corner : bounds.GetCorners())
                    samples.push_back(glm::mix(corner, center, 0.1f));

                auto countVisible = [&](vec3 cameraPosition) {

                    int32_t visible = 0;
                    for (auto sample : samples) {
                        auto toSample = sample - cameraPosition;
                        auto length = glm::length(toSample);
                        if (length < 1e-4f || !Raycast(*scene, cameraPosition, toSample / length, length, focused).valid)
                            visible++;
                    }
                    return visible;

                };

                auto bestDirection = direction;
                auto bestVisible = 0;
                for (auto offset : angleOffsets) {
                    auto candidate = GetDirection(azimuth + offset.x, glm::clamp(elevation + offset.y, -80.0f, 89.0f));

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
            auto entities = FindTopmostEntities(*scene, spec.entity);
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
            report.AddOutput(relative);
            report.Print("Captured " + relative + (phase == Phase::Play ? " at " + FormatTime(request.time) : ""));

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
                    report.AddOutput(fs::relative(passPath, job.outputDirectory).generic_string());
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

    void App::RunEvent(const InputEvent& input) {

        timeline.Fire(input);
        if (input.device == InputEvent::Device::Axis)
            report.Print(FormatTime(timeline.time) + " controller " + input.name + " " + FormatFloat(input.value, 2));
        else
            report.Print(FormatTime(timeline.time) + (input.device == InputEvent::Device::Key ? " key " : " controller ") + input.name + " " + input.action);

    }

    void App::RunEvent(const LookEvent& look) {

        auto entities = FindTopmostEntities(*scene, look.entity);
        if (entities.empty())
            report.Warn(FormatTime(timeline.time) + " look: no entity matches '" + look.entity + "'");
        for (auto entity : entities)
            SetLookDirection(entity, look.yaw, look.pitch);

    }

    void App::RunEvent(const TeleportEvent& teleport) {

        auto entities = FindTopmostEntities(*scene, teleport.entity);
        if (entities.empty())
            report.Warn(FormatTime(timeline.time) + " teleport: no entity matches '" + teleport.entity + "'");

        for (auto entity : entities) {
            if (auto player = entity.TryGetComponent<PlayerComponent>(); player && player->IsValid())
                player->SetPosition(teleport.position);
            SetWorldPosition(entity, teleport.position);
            if (teleport.yaw || teleport.pitch)
                SetLookDirection(entity, teleport.yaw, teleport.pitch);
            report.Print(FormatTime(timeline.time) + " teleported " + GetEntityLabel(entity) + " to " + FormatVec3(teleport.position));
        }

    }

    void App::RunEvent(const AnimationEvent& spec) {

        auto entities = FindTopmostEntities(*scene, spec.entity);
        if (entities.empty())
            report.Warn(FormatTime(timeline.time) + " animation: no entity matches '" + spec.entity + "'");

        ResourceHandle<Animation::Animation> animation;
        try {
            animation = LoadAnimationForScene(spec.animation, spec.model);
        }
        catch (const std::exception& e) {
            report.Warn(FormatTime(timeline.time) + " animation: " + e.what());
            return;
        }

        for (auto entity : entities) {
            auto animationComponent = entity.TryGetComponent<AnimationComponent>();
            if (!animationComponent) {
                report.Warn(FormatTime(timeline.time) + " animation: " + GetEntityLabel(entity) + " has no animation component");
                continue;
            }
            animationComponent->Play(animation, spec.blendTime);
            report.Print(FormatTime(timeline.time) + " playing " + animation.GetResource()->path + " on " + GetEntityLabel(entity));
        }

    }

    void App::RunEvent(const LogEvent& log) {

        report.Print(FormatTime(timeline.time) + " " + log.text);

    }

    void App::RunEvent(const ExpectEvent& expect) {

        bool passed = false;
        auto result = EvaluateExpectation(*scene, expect.expectation, passed);
        result["t"] = timeline.time;
        result["passed"] = passed;
        report.expectations.push_back(result);

        report.Print(FormatTime(timeline.time) + " expectation " + (passed ? "passed: " : "FAILED: ") + result.dump());

    }

    void App::RunEvent(const PauseEvent& pause) {

        // The job time continues, e.g. to check that nothing moves and scripts keep their state
        scene->paused = pause.pause;
        report.Print(FormatTime(timeline.time) + (scene->paused ? " paused the scene" : " resumed the scene"));

    }

    void App::RunEvent(const ResetScriptsEvent&) {

        // Leaving play mode discards non-permanent script environments; scene.paused keeps their state
        scene->physicsWorld->pauseSimulation = true;
        UpdateSceneWithCaptureCamera(*activeCamera, freezeDeltaTime);
        scene->physicsWorld->pauseSimulation = false;
        report.Print(FormatTime(timeline.time) + " reset the state of all scripts");

    }

    void App::RunEvent(const EndEvent&) {

        // The remaining timed events are skipped, conditional events that didn't happen yet still fail
        report.Print(FormatTime(timeline.time) + " ended the job");
        timeline.End();

    }

    void App::SetLookDirection(Scene::Entity entity, std::optional<float> yaw, std::optional<float> pitch) {

        auto camera = entity.TryGetComponent<CameraComponent>();
        if (!camera) {
            // Entities without a camera (e.g. driven by a script) are turned around the world Y axis instead
            auto transform = entity.TryGetComponent<TransformComponent>();
            if (!transform || !yaw) {
                report.Warn(GetEntityLabel(entity) + " has no camera, only the yaw of its transform can be set");
                return;
            }

            auto global = glm::translate(vec3(transform->globalMatrix[3])) * glm::rotate(glm::radians(*yaw), vec3(0.0f, 1.0f, 0.0f));
            transform->SetGlobal(global, scene->GetParentEntity(entity));
            report.Print(FormatTime(timeline.time) + " " + GetEntityLabel(entity) + " turned to yaw " + FormatFloat(*yaw, 1));
            return;
        }

        // Yaw 0 looks towards +Z, 90 towards +X
        if (yaw)
            camera->rotation.x = glm::radians(*yaw);
        if (pitch)
            camera->rotation.y = glm::radians(*pitch);

        report.Print(FormatTime(timeline.time) + " " + GetEntityLabel(entity) + " looks at yaw " +
            FormatFloat(glm::degrees(camera->rotation.x), 1) + ", pitch " +
            FormatFloat(glm::degrees(camera->rotation.y), 1));

    }

    void App::BuildOverlay() {

        overlay->Clear();
        if (!job.overlay)
            return;

        const auto& spec = *job.overlay;
        overlay->depthTest = spec.depthTest;
        auto& batch = overlay->GetBatch();
        auto line = [&](vec3 from, vec3 to, vec3 color) { batch->AddLine(from, to, color, color); };

        for (auto entity : FindTopmostEntities(*scene, spec.bounds))
            batch->AddLineAABB(GetEntityBounds(entity), vec3(1.0f, 0.85f, 0.1f));

        if (spec.players) {
            auto playerSubset = scene->GetSubset<PlayerComponent>();
            for (auto entity : playerSubset)
                batch->AddLineCylinder(GetEntityPosition(entity), 1.8f, 0.35f, vec3(0.1f, 0.9f, 1.0f));
        }

        if (spec.trails && tracker) {
            for (const auto& [label, samples] : tracker->GetSamples())
                for (size_t i = 1; i < samples.size(); i++)
                    line(samples[i - 1].position, samples[i].position, vec3(1.0f, 0.2f, 0.9f));
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
                            batch->AddLineCross(ToVec3(pick["position"]), 0.3f, vec3(1.0f, 1.0f, 0.2f));
                    continue;
                }
                if (!hit.is_object())
                    continue;

                auto isHit = hit.value("hit", false);
                if (type == "raycast") {
                    auto origin = ToVec3(query["origin"]);
                    auto end = isHit ? ToVec3(hit["position"]) :
                        origin + glm::normalize(ToVec3(query["direction"])) * query.value("maxDistance", 4096.0f);
                    line(origin, end, isHit ? vec3(0.2f, 1.0f, 0.2f) : vec3(1.0f, 0.2f, 0.2f));
                }
                if ((type == "raycast" || type == "ground") && isHit)
                    batch->AddLineCross(ToVec3(hit["position"]), 0.3f, vec3(0.2f, 1.0f, 0.2f));
                if (type == "path") {
                    const auto& samples = hit["samples"];
                    for (size_t i = 1; i < samples.size(); i++)
                        line(ToVec3(samples[i - 1]["position"]) + vec3(0.0f, 0.05f, 0.0f),
                            ToVec3(samples[i]["position"]) + vec3(0.0f, 0.05f, 0.0f), vec3(0.2f, 1.0f, 0.2f));
                    for (const auto& issue : hit["issues"])
                        batch->AddLineCross(ToVec3(issue["at"]), 0.4f, vec3(1.0f, 0.15f, 0.15f));
                }
                else if (type == "route") {
                    const auto& points = hit["points"];
                    for (size_t i = 1; i < points.size(); i++)
                        line(ToVec3(points[i - 1]) + vec3(0.0f, 0.3f, 0.0f), ToVec3(points[i]) + vec3(0.0f, 0.3f, 0.0f),
                            vec3(1.0f, 0.85f, 0.1f));
                }
            }
        }

        for (const auto& marker : spec.markers)
            batch->AddLineCross(ToVec3(marker.at("position")), marker.value("size", 0.5f),
                ToVec3(marker.value("color", json()), vec3(1.0f, 0.3f, 0.1f)));

        for (const auto& axes : spec.axes)
            batch->AddLineAxes(axes, 2.0f);

    }

}

Atlas::EngineInstance* GetEngineInstance() {

    return new Atlas::Capture::App();

}
