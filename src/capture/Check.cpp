#include "Check.h"
#include "LuaJob.h"
#include "MaterialGraphTools.h"
#include "Process.h"
#include "SceneFile.h"

#include <fstream>

namespace Atlas::Capture {

    struct EntityReference {

        std::string pattern;
        std::string where;
        // Entities that have to be in the scene file, others may be created by scripts or edits
        bool required = false;

    };

    static std::vector<EntityReference> CollectReferences(const Job& job) {

        std::vector<EntityReference> references;
        auto add = [&](const std::string& pattern, const std::string& where, bool required = false) {

            if (!pattern.empty() && pattern != "*" && !pattern.starts_with("#"))
                references.push_back({ pattern, where, required });

        };
        auto addCamera = [&](const CameraSpec& camera, const std::string& where, bool required = false) {

            if (camera.mode == "focus" || camera.mode == "follow")
                add(camera.entity, where, required);

        };

        for (const auto& shot : job.shots)
            addCamera(shot.camera, "shot " + shot.name, !job.edit);
        if (job.observer)
            addCamera(*job.observer, "observer");
        for (const auto& strip : job.strips)
            if (strip.hasCamera)
                addCamera(strip.camera, "strip " + strip.name);
        for (const auto& script : job.scripts)
            add(script.entity, "script " + script.path);

        for (const auto& event : job.timeline) {
            auto where = event.type + " at t=" + FormatFloat(event.time, 2);
            std::visit([&](const auto& action) {

                using Action = std::decay_t<decltype(action)>;
                if constexpr (std::is_same_v<Action, LookEvent> || std::is_same_v<Action, TeleportEvent> ||
                    std::is_same_v<Action, AnimationEvent>)
                    add(action.entity, where);
                if constexpr (std::is_same_v<Action, CaptureEvent>)
                    if (action.camera)
                        addCamera(*action.camera, where);
                if constexpr (std::is_same_v<Action, ExpectEvent>)
                    if (action.expectation.contains("entity"))
                        add(action.expectation["entity"].template get<std::string>(), where);

            }, event.action);
            if (event.when && event.when->contains("entity"))
                add((*event.when)["entity"].get<std::string>(), "condition of " + where);
        }

        if (job.checks.grounded)
            for (const auto& pattern : job.checks.grounded->entities)
                add(pattern, "checks.grounded");
        if (job.checks.penetration)
            for (const auto& pattern : job.checks.penetration->entities)
                add(pattern, "checks.penetration");
        if (job.track)
            for (const auto& pattern : job.track->entities)
                add(pattern, "track");

        return references;

    }

    static void CheckLua(const Job& job, std::vector<std::string>& issues) {

        auto check = [&](const std::string& code, const std::string& where, bool expression) {

            auto error = LuaJob::Compile(code, where, expression);
            if (!error.empty())
                issues.push_back("ERROR: Lua of the " + where + " doesn't compile: " + error);

        };

        for (const auto& event : job.timeline) {
            auto where = event.type + " event at t=" + FormatFloat(event.time, 2);
            if (auto lua = std::get_if<LuaEvent>(&event.action))
                check(lua->code, lua->file.empty() ? where : lua->file, !lua->result.empty());
            if (auto expect = std::get_if<ExpectEvent>(&event.action); expect && expect->expectation.contains("lua"))
                check(expect->expectation["lua"].get<std::string>(), where, true);
            if (event.when && event.when->contains("lua"))
                check((*event.when)["lua"].get<std::string>(), "condition of the " + where, true);
        }

        for (const auto& script : job.scripts) {
            fs::path path(script.path);
            std::ifstream stream(path.is_absolute() ? path : GetAssetDirectory() / path);
            if (stream.is_open())
                check(std::string(std::istreambuf_iterator<char>(stream), {}), "script " + script.path, false);
        }

    }

    static void RunPreflight(const Job& job, std::vector<std::string>& issues) {

        if (job.preflight.empty() || !job.play)
            return;

        auto log = GetExecutablePath().parent_path() / ".cache" / "capture-preflight.log";
        std::error_code error;
        fs::create_directories(log.parent_path(), error);
        auto start = std::chrono::steady_clock::now();
        auto exitCode = RunProcess(job.preflight, job.preflightDirectory, log, 120.0f);
        if (exitCode == 0)
            return;

        std::string command;
        for (const auto& arg : job.preflight)
            command += (command.empty() ? "" : " ") + arg;
        std::ifstream stream(log);
        std::vector<std::string> lines;
        for (std::string line; std::getline(stream, line);)
            if (!line.empty())
                lines.push_back(line);
        std::string output;
        for (size_t i = lines.size() > 12 ? lines.size() - 12 : 0; i < lines.size(); i++)
            output += "\n    " + lines[i];
        issues.push_back("ERROR: The preflight of the project (" + command + ") failed with exit code " + std::to_string(exitCode) +
            " after " + FormatFloat(SecondsSince(start), 1) + " s:" + output);

    }

    std::vector<std::string> CheckJob(const Job& job, bool preflight) {

        std::vector<std::string> issues;
        CheckLua(job, issues);
        if (job.edit)
            CheckMaterialGraphOperations(job.edit->operations, issues);

        auto references = CollectReferences(job);
        auto scenePath = fs::path(job.scene).is_absolute() ? fs::path(job.scene) : GetAssetDirectory() / job.scene;
        if (!references.empty() && fs::exists(scenePath)) {
            std::vector<std::string> names;
            try {
                names = ReadEntityNames(scenePath);
            }
            catch (const std::exception& e) {
                issues.push_back("NOTE: The entities of " + job.scene + " couldn't be read: " + e.what());
            }

            for (const auto& reference : references) {
                if (names.empty() || std::any_of(names.begin(), names.end(), [&](const std::string& name) {
                    return Helper::String::MatchWildcard(reference.pattern, name); }))
                    continue;

                std::string closest;
                for (const auto& name : FindClosestNames(names, reference.pattern))
                    closest += (closest.empty() ? "" : ", ") + std::string("'") + name + "'";
                auto message = "no entity of " + job.scene + " matches '" + reference.pattern + "' (" + reference.where + "), closest: " +
                    closest;
                issues.push_back(reference.required ? "ERROR: " + message : "NOTE: " + message + ". Scripts or edits may create it");
            }
        }
        else if (!fs::exists(scenePath)) {
            issues.push_back("NOTE: The scene " + job.scene + " doesn't exist yet");
        }

        if (preflight)
            RunPreflight(job, issues);

        std::stable_partition(issues.begin(), issues.end(), [](const std::string& issue) { return issue.starts_with("ERROR"); });
        return issues;

    }

    std::vector<std::string> CheckJobFile(const fs::path& path, const std::vector<std::string>& options) {

        std::vector<std::string> args = { "AtlasEngineCapture" };
        args.insert(args.end(), options.begin(), options.end());
        args.push_back(path.string());

        try {
            return CheckJob(ParseCommandLine(args));
        }
        catch (const std::exception& e) {
            return { std::string("ERROR: ") + e.what() };
        }

    }

}
