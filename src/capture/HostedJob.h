#pragma once

#include "Job.h"
#include "Player.h"
#include "Report.h"
#include "Checks.h"
#include "Image.h"

#include <memory>

namespace Atlas::Capture {

    // Runs a capture job inside a game host: the game loads, updates and renders its scene with its interface, the job
    // plays its timeline on it (keys, controller, mouse, Lua, expectations, conditions), runs its checks and captures the
    // window with the interface. Jobs without shots, edits, variants and capture cameras run, see README.md. A host:
    //   job = HostedJob::FromArguments(args, scenePath)    --capture <job.json> on the command line, nullptr without
    //   if (!job->KeepsPlayerDisplay())                    before the window and swap chain are set up: keep the window
    //                                                      and present without vsync, otherwise start like for a player
    //   job->Start(*scene, &luaState, windowId)            once the game runs, e.g. after its script started
    //   deltaTime = job->BeginFrame(deltaTime)             before the game update, returns the fixed time step of the job
    //   job->EndFrame()                                    after the scene update
    //   if (auto path = job->TakeCapture())                save the window to the path, the job waits until the file is there
    //   if (job->IsFinished()) Exit(job->GetExitCode())
    class HostedJob {

    public:
        // Throws std::runtime_error for jobs that need the renderer of the capture tool, and for jobs with the test display
        // until a job with the display of the player passed on this build of the game
        static std::unique_ptr<HostedJob> FromArguments(const std::vector<std::string>& args, const std::string& scenePath);

        bool KeepsPlayerDisplay() const { return job.playerDisplay; }

        void Start(Scene::Scene& scene, sol::state* luaState, uint32_t windowId);

        bool IsStarted() const { return scene != nullptr; }

        float BeginFrame(float deltaTime);

        void EndFrame();

        std::optional<fs::path> TakeCapture();

        bool IsFinished() const { return finished; }

        int32_t GetExitCode() const { return report.exitCode; }

    private:
        struct Capture {

            fs::path path;
            std::string group;
            std::string label;
            uintmax_t size = 0;

        };

        void CollectLog();

        void Finish();

        // Records the build of the game a job with the display of the player passed on
        static fs::path GetPlayerDisplayRecordPath();

        static int64_t GetBuildStamp();

        Job job;
        Report report;
        Player player;
        std::unique_ptr<CheckRunner> checks;
        std::unique_ptr<Tracker> tracker;
        Scene::Scene* scene = nullptr;

        std::vector<Capture> requested;
        std::optional<Capture> pending;
        bool takenThisFrame = false;
        std::vector<std::pair<std::string, std::string>> groups;
        std::vector<Ref<Image>> sheetImages;
        std::vector<std::string> sheetLabels;
        std::map<std::string, std::pair<std::vector<Ref<Image>>, std::vector<std::string>>> groupImages;

        float nextCheckTime = 0.0f;
        float nextTrackTime = 0.0f;
        bool fatal = false;
        // The first error ended the job, it finishes once its captures are written
        bool stopping = false;
        bool finished = false;
        std::chrono::steady_clock::time_point startTime;

    };

}
