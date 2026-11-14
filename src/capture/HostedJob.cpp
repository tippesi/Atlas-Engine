#include "HostedJob.h"
#include "SceneTools.h"
#include "Summary.h"
#include "Process.h"

#include "InstanceRegistry.h"

#include <SDL3/SDL.h>

#include <fstream>
#include <thread>

namespace Atlas::Capture {

    std::unique_ptr<HostedJob> HostedJob::FromArguments(const std::vector<std::string>& args, const std::string& scenePath) {

        auto option = std::find(args.begin(), args.end(), "--capture");
        if (option == args.end() || option + 1 == args.end())
            return nullptr;

        // The scene is the one of the game, the other options of the game aren't options of the job besides the GPU wait
        std::vector<std::string> jobArgs = { args.front(), "--scene", scenePath };
        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "--no-wait-gpu")
                jobArgs.push_back(args[i]);
            else if (args[i] == "--gpu-wait-timeout" && i + 1 < args.size())
                jobArgs.insert(jobArgs.end(), { args[i], args[i + 1] });
        }
        jobArgs.push_back(*(option + 1));

        auto hosted = std::make_unique<HostedJob>();
        auto& job = hosted->job;
        job = ParseCommandLine(jobArgs);

        auto unsupported = [](const std::string& what) {

            throw std::runtime_error(what + " needs the renderer of the capture tool, a job in a game plays the game with its camera");

        };
        if (!job.play)
            throw std::runtime_error("A job in a game plays it: set play and duration");
        if (job.edit)
            unsupported("edit");
        if (!job.shots.empty())
            unsupported("shots");
        if (!job.variants.empty())
            unsupported("variants");
        if (job.observer)
            unsupported("observer");
        if (job.inspect)
            unsupported("inspect");
        for (const auto& strip : job.strips)
            if (strip.hasCamera)
                unsupported("A strip with a camera");
        for (const auto& event : job.timeline) {
            if (auto capture = std::get_if<CaptureEvent>(&event.action); capture && (capture->camera || capture->still || !capture->crop.is_null()))
                unsupported("A capture with a camera, still or crop");
            if (std::holds_alternative<ResetScriptsEvent>(event.action))
                unsupported("resetScripts");
        }

        auto& report = hosted->report;
        report.Reset(job);
        report.showWindow = false;
        if (auto error = report.Begin(); !error.empty())
            throw std::runtime_error(error);
        hosted->startTime = std::chrono::steady_clock::now();

        // Jobs in a window without vsync never start the game like a player does, each build has to pass one that does.
        // The report of a refused job replaces the outputs of its previous run
        if (!job.playerDisplay) {
            std::ifstream record(GetPlayerDisplayRecordPath());
            int64_t passed = 0;
            auto build = GetBuildStamp();
            std::string error;
            if (build == 0)
                error = "The build time of the game can't be read, so jobs in a window can't run";
            else if (!(record >> passed) || passed != build)
                error = "This build of the game hasn't passed a job with \"playerDisplay\": true yet. It starts the game "
                    "with the display settings of a player (fullscreen, vsync); run one before the other jobs";
            if (!error.empty()) {
                hosted->fatal = true;
                report.Error(error);
                hosted->Finish();
                throw std::runtime_error(error);
            }
        }

        report.Print("Job '" + job.name + "' in the game, it starts with the game" + (report.previousSeconds > 0.0f ?
            ", the previous run took " + FormatFloat(report.previousSeconds, 0) + " s" : ""));
        // The game loads its world after this, scenes of other engine applications would leave too little GPU memory.
        // Its window exists already and has to handle its messages meanwhile
        if (job.waitGpu) {
            InstanceRegistry::QueueForGpu("waiting for the GPU to start job " + job.name);
            auto waitStart = std::chrono::steady_clock::now();
            auto nextPrint = waitStart;
            std::string waitingFor;
            std::vector<InstanceRegistry::Instance> ahead;
            while (!InstanceRegistry::PollGpu(ahead)) {
                auto seconds = SecondsSince(waitStart);
                auto reason = InstanceRegistry::Describe(ahead);
                if (!reason.empty() && (reason != waitingFor || std::chrono::steady_clock::now() >= nextPrint)) {
                    report.Print("Waiting for the GPU (" + FormatFloat(seconds, 0) + " s): " + reason);
                    nextPrint = std::chrono::steady_clock::now() + std::chrono::seconds(60);
                    waitingFor = reason;
                }
                if (seconds > job.gpuWaitTimeout) {
                    auto error = "The GPU is still busy after " + FormatFloat(seconds, 0) + " s (gpuWaitTimeout), waiting for " + waitingFor;
                    hosted->fatal = true;
                    report.Error(error);
                    hosted->Finish();
                    throw std::runtime_error(error);
                }
                for (int32_t i = 0; i < 10; i++) {
                    SDL_PumpEvents();
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            }
            // The wait doesn't count into the duration of the job
            hosted->startTime += std::chrono::steady_clock::now() - waitStart;
        }
        InstanceRegistry::SetActivity("job " + job.name, true);
        return hosted;

    }

    void HostedJob::Start(Scene::Scene& playedScene, sol::state* luaState, uint32_t windowId) {

        scene = &playedScene;
        try {
            if (!job.settings.empty())
                ApplySettings(*scene, job.settings);

            if (!job.queries.empty()) {
                auto results = RunQueries(*scene, job.queries, {}, job.outputDirectory);
                report.WriteJson("queries.json", results);
                for (const auto& result : results)
                    report.Print("Query " + result["query"].value("type", "") + " -> " +
                        (result.contains("error") ? result["error"].dump() : result["result"].dump()).substr(0, 300));
            }

            checks = std::make_unique<CheckRunner>(job.checks);
            if (job.track)
                tracker = std::make_unique<Tracker>(*job.track);

            // Strips are live captures of the window
            auto events = job.timeline;
            for (const auto& strip : job.strips) {
                auto group = SanitizeFileName(strip.name);
                groups.emplace_back(group, strip.name);
                auto count = int32_t(std::floor((strip.to - strip.from) / strip.every + 1e-3f));
                for (int32_t i = 0; i <= count; i++) {
                    auto time = strip.from + float(i) * strip.every;
                    CaptureEvent capture;
                    capture.file = group + "/" + FormatFloat(time, 2) + "s.png";
                    capture.group = group;
                    events.push_back({ .time = time, .type = "capture", .action = capture });
                }
            }

            player.Reset(job);
            player.Start(job, *scene, std::move(events), report, luaState, windowId);
            report.Print("Playing for " + FormatFloat(job.duration, 2) + "s in the game");
        }
        catch (const std::exception& e) {
            report.Error(e.what());
            fatal = true;
        }

    }

    float HostedJob::BeginFrame(float deltaTime) {

        if (finished || !scene)
            return deltaTime;

        try {
            CollectLog();

            // A capture is done once its file stopped growing
            if (pending) {
                std::error_code error;
                auto size = fs::exists(pending->path, error) ? fs::file_size(pending->path, error) : 0;
                if (size > 0 && size == pending->size) {
                    auto relative = fs::relative(pending->path, job.outputDirectory).generic_string();
                    report.AddOutput(relative);
                    report.Print("Captured " + relative + " at " + FormatTime(player.timeline.time));
                    if (auto image = LoadImageFile(pending->path)) {
                        auto small = Downscale(image, 640);
                        sheetImages.push_back(small);
                        sheetLabels.push_back(fs::path(relative).replace_extension().generic_string());
                        if (!pending->group.empty()) {
                            groupImages[pending->group].first.push_back(small);
                            groupImages[pending->group].second.push_back(pending->label);
                        }
                    }
                    pending.reset();
                }
                else if (pending) {
                    pending->size = size;
                }
            }

            if (!fatal && !stopping) {
                player.Update([&](const EventSpec& event) {

                    const auto& capture = std::get<CaptureEvent>(event.action);
                    auto file = capture.file.value_or(SanitizeFileName(capture.name) + "_" + FormatFloat(player.timeline.time, 2) + "s.png");
                    requested.push_back({ job.outputDirectory / file, capture.group, capture.label.value_or(FormatTime(event.time)) });
                    fs::create_directories(requested.back().path.parent_path());
                    return Player::EventResult::Done;

                });
            }

            auto done = fatal || stopping || player.timeline.IsFinished(job.duration);
            if (done && requested.empty() && !pending) {
                if (!fatal && !stopping) {
                    player.ReportMissedEvents(*checks);
                    checks->Run(*scene, player.timeline.time);
                    if (tracker)
                        tracker->Record(*scene, player.timeline.time);
                }
                Finish();
                return deltaTime;
            }
        }
        catch (const std::exception& e) {
            report.Error(e.what());
            fatal = true;
        }

        return job.fixedDeltaTime;

    }

    void HostedJob::EndFrame() {

        if (finished || !scene || fatal || stopping)
            return;

        auto& timeline = player.timeline;
        timeline.time += job.fixedDeltaTime;
        if (timeline.IsDue(nextCheckTime)) {
            checks->Run(*scene, timeline.time);
            nextCheckTime += job.checks.interval;
        }
        if (tracker && timeline.IsDue(nextTrackTime)) {
            tracker->Record(*scene, timeline.time);
            nextTrackTime += tracker->spec.interval;
        }

    }

    std::optional<fs::path> HostedJob::TakeCapture() {

        if (finished || pending || requested.empty())
            return std::nullopt;

        pending = requested.front();
        requested.erase(requested.begin());
        return pending->path;

    }

    void HostedJob::CollectLog() {

        for (const auto& entry : report.CollectLog()) {
            player.timeline.MatchLog(entry.message);
            if (entry.type != Log::TYPE_ERROR || !checks || !job.checks.logErrors || MatchAnyPattern(job.checks.logIgnore, entry.message))
                continue;

            checks->AddFailure({ "log", entry.message.substr(0, 160), player.timeline.time, entry.message });
            if (job.checks.failFast && !stopping) {
                report.Print("Stopping at the first error, checks.failFast = false runs to the end");
                stopping = true;
            }
        }

    }

    void HostedJob::Finish() {

        for (const auto& [group, title] : groups) {
            const auto& [images, labels] = groupImages[group];
            if (!images.empty() && WriteImage(ComposeSheet(images, labels, 0, title), job.outputDirectory / (group + ".png")))
                report.AddOutput(group + ".png");
        }
        if (!sheetImages.empty()) {
            auto columns = int32_t(std::ceil(std::sqrt(float(sheetImages.size()))));
            auto cellWidth = std::max((1536 - 4 * (columns + 1)) / columns, 64);
            if (WriteImage(ComposeSheet(sheetImages, sheetLabels, columns, job.name, cellWidth), job.outputDirectory / "sheet.jpg"))
                report.AddOutput("sheet.jpg");
        }
        if (tracker)
            report.WriteJson("trace.json", tracker->GetTrace());

        auto failedExpectations = std::count_if(report.expectations.begin(), report.expectations.end(),
            [](const json& expectation) { return !expectation["passed"].get<bool>(); });
        report.exitCode = fatal ? 2 : (checks && checks->HasFailures()) || failedExpectations > 0 ? 1 : 0;

        if (auto build = GetBuildStamp(); job.playerDisplay && report.exitCode == 0 && build != 0) {
            auto recordPath = GetPlayerDisplayRecordPath();
            std::error_code error;
            fs::create_directories(recordPath.parent_path(), error);
            std::ofstream(recordPath) << build;
        }

        std::string status = report.exitCode == 0 ? "passed" : report.exitCode == 1 ? "failed" : "error";
        json result = {
            { "job", job.name }, { "runId", report.runId }, { "scene", job.scene }, { "status", status }, { "exitCode", report.exitCode },
            { "host", true }, { "errors", report.errors }, { "warnings", report.warnings }, { "engineLogErrors", report.logErrorCount },
            { "totalSeconds", SecondsSince(startTime) }, { "playedSeconds", player.timeline.time }
        };
        if (fatal)
            result["reason"] = report.errors.empty() ? "unknown error" : report.errors.front();
        else if (report.exitCode == 1)
            result["reason"] = std::to_string(checks ? checks->GetSummary().size() : 0) + " failed checks, " +
                std::to_string(failedExpectations) + " failed expectations";
        if (checks)
            result["checks"] = checks->GetReport();
        if (!report.expectations.empty())
            result["expectations"] = report.expectations;
        if (!report.results.empty())
            result["results"] = report.results;
        if (auto triggers = player.timeline.GetTriggerReport(); !triggers.is_null())
            result["conditionalEvents"] = triggers;
        report.Write(result);

        for (const auto& line : FormatSummary(report.data, job.outputDirectory))
            report.Print(line);
        report.End();
        player.Stop();
        finished = true;

    }

    fs::path HostedJob::GetPlayerDisplayRecordPath() {

        auto executable = GetExecutablePath();
        return executable.parent_path() / ".cache" / ("capture-player-display-" + executable.stem().string() + ".txt");

    }

    int64_t HostedJob::GetBuildStamp() {

        // 0 if the time can't be read, the error value would be the same for every build
        std::error_code error;
        auto time = fs::last_write_time(GetExecutablePath(), error);
        return error ? 0 : int64_t(time.time_since_epoch().count());

    }

}
