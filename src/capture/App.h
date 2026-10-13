#pragma once

#include "Job.h"
#include "CaptureRenderer.h"
#include "Checks.h"
#include "SceneTools.h"
#include "Overlay.h"

#include "EngineInstance.h"
#include "input/PlayerInput.h"

#include <chrono>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>

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

        struct Trigger {

            EventSpec event;
            bool logMatched = false;
            std::optional<float> metTime;
            bool fired = false;

        };

        struct StillState {

            bool active = false;
            CameraSpec camera;
            CaptureRequest request;
            int32_t settleFrames = 0;
            int32_t settledFrames = 0;
            bool requested = false;

        };

        struct ValidationMessage {
            std::string severity;
            std::string message;
            int32_t count = 0;
        };

        void Fail(const std::string& message);

        void Warn(const std::string& message);

        void Print(const std::string& message);

        void CollectLog();

        bool IsSceneReady();

        void Prepare();

        void FinishEdit();

        void UpdateShots();

        void StartPlay();

        void UpdatePlay();

        void UpdateStill();

        // Checks the conditions of conditional events and queues the events that are due
        void UpdateTriggers();

        // Writes the GPU timings averaged over the last frames to profile.json and prints the most expensive ones
        void WriteProfile();

        // Writes the validation messages grouped by their id to validation.json
        void WriteValidationReport();

        void Finish();

        // Updates the scene while the capture camera is the main camera. A negative delta
        // time uses the fixed delta time of the job
        void UpdateSceneWithCaptureCamera(const CameraSpec& spec, float deltaTime = -1.0f);

        void ApplyCameraSpec(const CameraSpec& spec);

        bool UsesCaptureCamera(const CameraSpec& spec) const;

        void RequestCapture(const CaptureRequest& request);

        bool HasPendingCaptures() const;

        void ProcessReadback();

        void AddToSheet(const std::string& group, const Ref<Image>& image, const std::string& label);

        void WriteJson(const std::string& fileName, const json& j);

        void RunExpectation(const EventSpec& event);

        void FireKey(const std::string& key, bool down);

        void BuildOverlay();

        void SetLookDirection(Scene::Entity entity, const json& look);

        // Presents the frame, in a shown window with a status line and the latest output below it
        void PresentWindow();

        std::string GetStatusLine() const;

        static VKAPI_ATTR VkBool32 VKAPI_CALL ValidationCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* data, void* userData);

        Job job;
        Phase phase = Phase::Loading;
        bool fatal = false;

        Ref<Scene::Scene> scene;
        Scene::Entity captureCameraEntity;

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
        int32_t editFrames = 0;

        // Capture path -> (sheet group, label) for the comparison of variants
        std::map<std::string, std::pair<std::string, std::string>> variantCaptures;
        std::map<std::string, CameraSpec> stripCameras;

        float playTime = 0.0f;
        size_t nextEvent = 0;
        std::vector<EventSpec> events;
        std::vector<Trigger> triggers;
        std::vector<std::pair<float, std::string>> pendingKeyReleases;
        float nextCheckTime = 0.0f;
        float nextTrackTime = 0.0f;

        Input::PlayerInput playerInput;
        std::unique_ptr<Overlay> overlay;
        json queryResults = json::array();

        std::unique_ptr<CheckRunner> checkRunner;
        std::unique_ptr<Tracker> tracker;

        json report;
        json outputs = json::array();
        json expectations = json::array();
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        bool expectationFailed = false;

        std::ofstream logFile;
        size_t logIndex = 0;
        int32_t logErrorCount = 0;

        // Message id -> message, the validation layers call back from any thread
        VkDebugUtilsMessengerEXT validationMessenger = VK_NULL_HANDLE;
        std::map<std::string, ValidationMessage> validationMessages;
        std::mutex validationMutex;

        int32_t frameCount = 0;
        int32_t readyFrame = -1;
        int32_t rtWaitFrames = 0;
        std::chrono::steady_clock::time_point startTime;
        float loadSeconds = 0.0f;

        // Shown window: output panel and playback in real time
        static constexpr size_t outputLineCount = 12;
        Ref<Font> font;
        std::deque<CaptureRenderer::TextLine> outputLines;
        std::chrono::steady_clock::time_point playStartTime;

    };

}
