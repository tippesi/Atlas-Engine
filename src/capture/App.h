#pragma once

#include "Job.h"
#include "CaptureRenderer.h"
#include "Checks.h"
#include "SceneTools.h"
#include "Overlay.h"
#include "Pipeline.h"
#include "Report.h"
#include "SceneEditor.h"
#include "Server.h"
#include "Timeline.h"

#include "EngineInstance.h"
#include "input/PlayerInput.h"

#include <chrono>
#include <map>
#include <set>

namespace Atlas::Capture {

    class App : public EngineInstance {

    public:
        App() : EngineInstance("Atlas Engine Capture", 1280, 720, AE_WINDOW_HIDDEN) {}

        void LoadContent() final;

        void UnloadContent() final;

        void Update(float deltaTime) final;

        void Render(float deltaTime) final;

    private:
        enum class Phase {
            // Server mode: waiting for the next command
            Idle,
            Loading,
            Prepare,
            EditSave,
            Shots,
            Play,
            Finish,
            Done
        };

        struct CaptureRequest {

            std::string group;
            std::string label;
            fs::path path;
            std::vector<std::string> passes;
            float time = 0.0f;

        };

        struct SheetGroup {

            std::string title;
            int32_t columns = 0;
            fs::path path;
            std::vector<Ref<Image>> images;
            std::vector<std::string> labels;

        };

        struct ShotState {

            size_t variant = 0;
            size_t shot = 0;
            size_t view = 0;
            int32_t settledFrames = 0;
            bool requested = false;

        };

        struct StillState {

            bool active = false;
            CameraSpec camera;
            CaptureRequest request;
            int32_t settleFrames = 0;
            int32_t settledFrames = 0;
            bool requested = false;

        };

        // A single job (the default), a server that processes commands or a pipeline of edit jobs
        enum class Mode {
            Job,
            Server,
            Pipeline
        };

        // Sets up a job, it uses the loaded scene if it is the scene of the job
        void StartJob(const Job& nextJob);

        // Parses a job file with the options of the command line that apply to every job
        Job ParseJobFile(const fs::path& path, const std::vector<std::string>& extraArguments = {}) const;

        // Resets everything that belongs to a single job
        void ResetJobState();

        void LoadScene(const std::string& path);

        void UnloadScene();

        // Server mode: reads the next command file, runs commands that don't need frames immediately
        void ProcessNextCommand();

        void LoadSceneCommand(const json& command);

        void ReloadSceneCommand(const json& command);

        // Loads a scene without a job, it stays loaded for the following jobs. The path is a copy, it can be the
        // path of the loaded scene, which is unloaded first
        void LoadSceneForCommand(std::string path, const json& command);

        void UnloadSceneCommand(const json& command);

        void RunCommand(const json& command);

        void StopCommand(const json& command);

        // Server mode: writes <command>.result.json, the result of the current command
        void CompleteCommand(const json& result);

        // Called once the report of a job is written: exits, continues the pipeline or waits for the next command
        void FinishJob();

        void StartPipelineStep();

        // A pipeline step continues with the scene in memory if it reads the scene the step before saved
        bool StepReadsScene(size_t step, const std::string& scene);

        void Fail(const std::string& message);

        // Engine log entries can fail the checks and meet the log conditions of the timeline
        void CollectLog();

        bool IsSceneReady();

        void Prepare();

        void FinishEdit();

        void UpdateShots();

        void StartPlay();

        void UpdatePlay();

        void UpdateStill();

        void Finish();

        void CreateCaptureCamera();

        // Updates the scene while the capture camera is the main camera. A negative delta
        // time uses the fixed delta time of the job
        void UpdateSceneWithCaptureCamera(const CameraSpec& spec, float deltaTime = -1.0f);

        void ApplyCameraSpec(const CameraSpec& spec);

        bool UsesCaptureCamera(const CameraSpec& spec) const;

        void RequestCapture(const CaptureRequest& request);

        bool HasPendingCaptures() const;

        void ProcessReadback();

        void AddToSheet(const std::string& group, const Ref<Image>& image, const std::string& label);

        // Timeline events, captures are handled by UpdatePlay
        void RunEvent(const InputEvent& input);

        void RunEvent(const LookEvent& look);

        void RunEvent(const TeleportEvent& teleport);

        void RunEvent(const AnimationEvent& animation);

        void RunEvent(const LogEvent& log);

        void RunEvent(const ExpectEvent& expect);

        void RunEvent(const PauseEvent& pause);

        void RunEvent(const ResetScriptsEvent&);

        void RunEvent(const EndEvent&);

        void BuildOverlay();

        void SetLookDirection(Scene::Entity entity, std::optional<float> yaw, std::optional<float> pitch);

        // Presents the frame, in a shown window with a status line and the latest output below it
        void PresentWindow();

        std::string GetStatusLine() const;

        // Render scale and upscaler, e.g. ", render scale 0.75 (FSR2)"
        std::string GetRenderDescription() const;

        Mode mode = Mode::Job;
        // Options of the command line which are passed on to every job of a server or pipeline
        std::vector<std::string> jobArguments;

        Job job;
        Phase phase = Phase::Loading;
        bool fatal = false;

        Ref<Scene::Scene> scene;
        // Asset path of the loaded scene and its settings right after loading, restored for each job
        std::string scenePath;
        json sceneSettings;
        // A scene loaded by a command or kept between pipeline steps stays loaded after the job
        bool keepScene = false;
        // Set while a loadScene command waits for the scene to finish loading
        bool loadingForCommand = false;
        // The meshes of a reloaded scene, with their materials and textures, stay loaded until it is ready again
        std::vector<ResourceHandle<Mesh::Mesh>> retainedMeshes;
        Scene::Entity captureCameraEntity;
        // Set by a scene update, the next render uses it
        bool sceneUpdated = false;
        std::chrono::steady_clock::time_point loadStartTime;

        std::optional<Server> server;
        std::optional<Pipeline> pipeline;

        bool rendererInitialized = false;
        float defaultRenderScale = 1.0f;

        CaptureRenderer captureRenderer;

        std::optional<CameraSpec> activeCamera;

        // All requests of a frame share the same rendered image
        std::vector<CaptureRequest> captureRequests;
        std::vector<CaptureRequest> pendingReadbacks;
        std::map<std::string, SheetGroup> sheets;
        std::vector<std::string> sheetOrder;

        ShotState shotState;
        StillState stillState;
        json baseSettings;
        float baseRenderScale = 1.0f;
        int32_t editFrames = 0;
        EditState editState;

        // Capture path -> (sheet group, label) for the comparison of variants
        std::map<std::string, std::pair<std::string, std::string>> variantCaptures;
        std::map<std::string, CameraSpec> stripCameras;

        Timeline timeline;
        float nextCheckTime = 0.0f;
        float nextTrackTime = 0.0f;

        Input::PlayerInput playerInput;
        std::unique_ptr<Overlay> overlay;
        json queryResults = json::array();

        std::unique_ptr<CheckRunner> checkRunner;
        std::unique_ptr<Tracker> tracker;

        Report report;

        int32_t frameCount = 0;
        int32_t readyFrame = -1;
        int32_t rtWaitFrames = 0;
        std::chrono::steady_clock::time_point startTime;
        float loadSeconds = 0.0f;

        // Shown window: output panel and playback in real time
        static constexpr size_t outputLineCount = 12;
        Ref<Font> font;
        std::chrono::steady_clock::time_point playStartTime;

    };

}
