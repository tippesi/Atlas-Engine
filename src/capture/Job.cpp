#include "Job.h"

#include <fstream>
#include <set>
#include <stdexcept>

namespace Atlas::Capture {

    static void CheckKeys(const json& j, const std::set<std::string>& allowed, const std::string& context) {

        if (!j.is_object())
            throw std::runtime_error(context + " must be a JSON object");

        // Typos in job files would otherwise be silently ignored
        for (auto& [key, value] : j.items()) {
            if (!allowed.contains(key))
                throw std::runtime_error("Unknown key '" + key + "' in " + context);
        }

    }

    static std::vector<std::string> ToStringList(const json& j) {

        std::vector<std::string> list;
        if (j.is_string())
            list.push_back(j.get<std::string>());
        else if (j.is_array())
            for (const auto& item : j)
                list.push_back(item.get<std::string>());

        return list;

    }

    static std::vector<std::string> SplitList(const std::string& text, char separator = ',') {

        std::vector<std::string> list;
        size_t start = 0;
        while (start <= text.size()) {
            auto end = text.find(separator, start);
            if (end == std::string::npos)
                end = text.size();
            auto item = text.substr(start, end - start);
            if (!item.empty())
                list.push_back(item);
            start = end + 1;
        }

        return list;

    }

    static json ParseVec3Argument(const std::string& text, const std::string& option) {

        auto parts = SplitList(text);
        if (parts.size() != 3)
            throw std::runtime_error(option + " expects x,y,z but got '" + text + "'");

        return json::array({ std::stof(parts[0]), std::stof(parts[1]), std::stof(parts[2]) });

    }

    static ShotSpec ParseShot(const json& j, size_t index) {

        CheckKeys(j, { "name", "camera", "views", "passes", "settleFrames" }, "shot");

        ShotSpec shot;
        shot.name = j.value("name", "shot" + std::to_string(index));
        if (j.contains("camera"))
            shot.camera = ParseCamera(j["camera"]);
        if (j.contains("views"))
            shot.views = ToStringList(j["views"]);
        if (j.contains("passes"))
            shot.passes = ToStringList(j["passes"]);
        shot.settleFrames = j.value("settleFrames", -1);

        return shot;

    }

    static GroundedCheckSpec ParseGrounded(const json& j) {

        CheckKeys(j, { "entities", "maxGap", "maxSink", "surfaces" }, "checks.grounded");

        GroundedCheckSpec spec;
        spec.entities = ToStringList(j.at("entities"));
        spec.maxGap = j.value("maxGap", spec.maxGap);
        spec.maxSink = j.value("maxSink", spec.maxSink);
        if (j.contains("surfaces"))
            spec.surfaces = ToStringList(j["surfaces"]);
        return spec;

    }

    static PenetrationCheckSpec ParsePenetration(const json& j) {

        CheckKeys(j, { "entities", "against", "margin", "minTriangles" }, "checks.penetration");

        PenetrationCheckSpec spec;
        spec.entities = ToStringList(j.at("entities"));
        spec.against = j.contains("against") ? ToStringList(j["against"]) : std::vector<std::string> { "*" };
        spec.margin = j.value("margin", spec.margin);
        spec.minTriangles = j.value("minTriangles", spec.minTriangles);
        return spec;

    }

    static const std::set<std::string> eventTypes = { "capture", "key", "look", "teleport", "log", "expect",
        "resetScripts", "pause" };

    CameraSpec ParseCamera(const json& j) {

        CameraSpec camera;

        if (j.is_string()) {
            camera.mode = j.get<std::string>();
            return camera;
        }

        CheckKeys(j,
            { "mode", "position", "lookAt", "yaw", "pitch", "entity", "view", "azimuth", "elevation", "local",
                "distance", "padding", "avoidOcclusion", "dynamic", "offset", "fov", "near", "far" },
            "camera");

        // Infer the mode from the given keys if it isn't specified
        if (j.contains("mode"))
            camera.mode = j["mode"].get<std::string>();
        else if (j.contains("entity") && j.contains("offset"))
            camera.mode = "follow";
        else if (j.contains("entity"))
            camera.mode = "focus";
        else if (j.contains("position"))
            camera.mode = "manual";

        static const std::set<std::string> modes = { "scene", "manual", "focus", "follow", "overview" };
        if (!modes.contains(camera.mode))
            throw std::runtime_error("Unknown camera mode '" + camera.mode + "'");

        camera.position = ToVec3(j.value("position", json()), camera.position);
        if (j.contains("lookAt"))
            camera.lookAt = ToVec3(j["lookAt"]);
        camera.yaw = j.value("yaw", camera.yaw);
        camera.pitch = j.value("pitch", camera.pitch);
        camera.entity = j.value("entity", camera.entity);
        camera.view = j.value("view", camera.view);
        if (j.contains("azimuth"))
            camera.azimuth = j["azimuth"].get<float>();
        if (j.contains("elevation"))
            camera.elevation = j["elevation"].get<float>();
        camera.local = j.value("local", camera.local);
        camera.distance = j.value("distance", camera.distance);
        camera.padding = j.value("padding", camera.padding);
        camera.avoidOcclusion = j.value("avoidOcclusion", camera.avoidOcclusion);
        camera.dynamic = j.value("dynamic", camera.dynamic);
        camera.offset = ToVec3(j.value("offset", json()), camera.offset);
        camera.fieldOfView = j.value("fov", camera.fieldOfView);
        camera.nearPlane = j.value("near", camera.nearPlane);
        camera.farPlane = j.value("far", camera.farPlane);

        if ((camera.mode == "focus" || camera.mode == "follow") && camera.entity.empty())
            throw std::runtime_error("Camera mode '" + camera.mode + "' requires an entity pattern");

        return camera;

    }

    Job ParseJob(const json& j, const fs::path& baseDirectory) {

        CheckKeys(j,
            { "name", "scene", "out", "resolution", "settleFrames", "loadTimeout", "fixedDt", "showWindow", "settings",
                "variants", "inspect", "queries", "edit", "shots", "play", "duration", "timeline", "strips", "observer",
                "scripts", "track", "overlay", "checks", "profile", "$schema", "description" },
            "job");

        Job job;

        if (!j.contains("scene"))
            throw std::runtime_error("The job doesn't specify a scene");

        job.scene = ToAssetPath(j["scene"].get<std::string>());
        job.name = j.value("name", fs::path(job.scene).stem().string());

        if (j.contains("out")) {
            fs::path out(j["out"].get<std::string>());
            job.outputDirectory = out.is_absolute() ? out : (baseDirectory / out).lexically_normal();
        }
        else {
            // bin/<build type>/../capture/<name>, the bin directory is ignored by git
            job.outputDirectory =
                (fs::current_path() / ".." / "capture" / SanitizeFileName(job.name)).lexically_normal();
        }

        if (j.contains("resolution")) {
            auto& resolution = j["resolution"];
            job.resolution = ivec2(resolution.at(0).get<int32_t>(), resolution.at(1).get<int32_t>());
            if (job.resolution.x <= 0 || job.resolution.y <= 0)
                throw std::runtime_error("Invalid resolution");
        }

        job.settleFrames = j.value("settleFrames", job.settleFrames);
        job.loadTimeout = j.value("loadTimeout", job.loadTimeout);
        job.fixedDeltaTime = j.value("fixedDt", job.fixedDeltaTime);
        job.showWindow = j.value("showWindow", job.showWindow);

        if (job.fixedDeltaTime <= 0.0f)
            throw std::runtime_error("fixedDt must be positive");

        if (j.contains("settings"))
            job.settings = j["settings"];

        if (j.contains("variants")) {
            for (const auto& variant : j["variants"]) {
                CheckKeys(variant, { "name", "settings" }, "variant");
                job.variants.push_back(
                    { variant.at("name").get<std::string>(), variant.value("settings", json::object()) });
            }
        }

        if (j.contains("inspect")) {
            auto& inspect = j["inspect"];
            InspectSpec spec;
            if (inspect.is_string()) {
                spec.file = inspect.get<std::string>();
            }
            else if (inspect.is_object()) {
                CheckKeys(inspect, { "file", "filter", "detail" }, "inspect");
                spec.file = inspect.value("file", spec.file);
                if (inspect.contains("filter"))
                    spec.filter = ToStringList(inspect["filter"]);
                if (inspect.contains("detail"))
                    spec.detail = ToStringList(inspect["detail"]);
            }
            if (!inspect.is_boolean() || inspect.get<bool>())
                job.inspect = spec;
        }

        if (j.contains("queries"))
            job.queries = j["queries"];

        if (j.contains("edit")) {
            auto& edit = j["edit"];
            CheckKeys(edit, { "operations", "output", "overwrite", "saveUnchanged" }, "edit");
            EditSpec spec;
            spec.operations = edit.value("operations", json::array());
            spec.output = edit.contains("output") ? ToAssetPath(edit["output"].get<std::string>()) : "";
            spec.overwrite = edit.value("overwrite", false);
            spec.saveUnchanged = edit.value("saveUnchanged", false);
            job.edit = spec;
        }

        if (j.contains("shots")) {
            size_t index = 0;
            for (const auto& shot : j["shots"])
                job.shots.push_back(ParseShot(shot, index++));
        }

        job.play = j.value("play", false);
        job.duration = j.value("duration", 0.0f);

        if (j.contains("timeline")) {
            for (const auto& event : j["timeline"]) {
                EventSpec spec;
                spec.time = event.value("t", 0.0f);
                spec.data = event;

                for (const auto& type : eventTypes) {
                    if (event.contains(type)) {
                        if (!spec.type.empty())
                            throw std::runtime_error("A timeline event can only have one type: " + event.dump());
                        spec.type = type;
                    }
                }
                if (spec.type.empty())
                    throw std::runtime_error(
                        "Timeline event without a known type (capture, key, look, teleport, log, expect, resetScripts, pause): " +
                        event.dump());
                if (spec.type == "pause" && !event["pause"].is_boolean())
                    throw std::runtime_error("A pause event needs true or false: " + event.dump());

                if (event.contains("when")) {
                    const auto& when = event["when"];
                    bool isLog = when.is_object() && when.contains("log") && when["log"].is_string();
                    bool isProperty = when.is_object() && when.contains("entity") && when.contains("property");
                    if (!isLog && !isProperty)
                        throw std::runtime_error(
                            "A when condition needs a log text or entity, property, op and value " "like an expectation: " +
                            event.dump());
                    spec.when = when;
                    spec.delay = event.value("delay", 0.0f);
                    spec.optional = event.value("optional", false);
                }

                job.timeline.push_back(spec);
            }
        }

        if (j.contains("strips")) {
            for (const auto& strip : j["strips"]) {
                CheckKeys(strip, { "name", "from", "to", "every", "camera", "columns" }, "strip");
                StripSpec spec;
                spec.name = strip.value("name", "strip" + std::to_string(job.strips.size()));
                spec.from = strip.value("from", 0.0f);
                spec.to = strip.value("to", job.duration);
                spec.every = strip.value("every", spec.every);
                spec.columns = strip.value("columns", 0);
                if (strip.contains("camera")) {
                    spec.camera = ParseCamera(strip["camera"]);
                    spec.hasCamera = true;
                }
                if (spec.every <= 0.0f)
                    throw std::runtime_error("strip.every must be positive");
                job.strips.push_back(spec);
            }
        }

        if (j.contains("observer"))
            job.observer = ParseCamera(j["observer"]);

        if (j.contains("scripts")) {
            for (const auto& script : j["scripts"]) {
                ScriptSpec spec;
                if (script.is_string()) {
                    spec.path = script.get<std::string>();
                }
                else {
                    CheckKeys(script, { "script", "entity" }, "scripts");
                    spec.path = script.at("script").get<std::string>();
                    spec.entity = script.value("entity", std::string());
                }

                // Scripts inside data/ are referenced like any other asset, others by their absolute path
                fs::path path(spec.path);
                auto resolved = path.is_absolute() ? path : (baseDirectory / path).lexically_normal();
                if (fs::exists(resolved)) {
                    auto assetPath = ToAssetPath(resolved.string());
                    spec.path = fs::path(assetPath).is_absolute() ? resolved.generic_string() : assetPath;
                }
                else if (!fs::exists(GetAssetDirectory() / path)) {
                    throw std::runtime_error("Script " + spec.path + " doesn't exist");
                }

                job.scripts.push_back(spec);
            }
        }

        if (j.contains("track")) {
            auto& track = j["track"];
            TrackSpec spec;
            if (track.is_object()) {
                CheckKeys(track, { "entities", "interval" }, "track");
                spec.entities = ToStringList(track.at("entities"));
                spec.interval = track.value("interval", spec.interval);
            }
            else {
                spec.entities = ToStringList(track);
            }
            job.track = spec;
        }

        if (j.contains("profile")) {
            auto& profile = j["profile"];
            if (profile.is_object()) {
                CheckKeys(profile, { "frames" }, "profile");
                job.profileFrames = std::max(profile.value("frames", 32), 1);
            }
            else {
                job.profileFrames = profile.get<bool>() ? 32 : 0;
            }
        }

        if (j.contains("overlay")) {
            auto& overlay = j["overlay"];
            OverlaySpec spec;
            if (overlay.is_object()) {
                CheckKeys(
                    overlay, { "bounds", "markers", "axes", "players", "trails", "queries", "depthTest" }, "overlay");
                if (overlay.contains("bounds"))
                    spec.bounds = ToStringList(overlay["bounds"]);
                spec.markers = overlay.value("markers", json::array());
                for (const auto& axes : overlay.value("axes", json::array()))
                    spec.axes.push_back(ToVec3(axes));
                spec.players = overlay.value("players", spec.players);
                spec.trails = overlay.value("trails", spec.trails);
                spec.queries = overlay.value("queries", spec.queries);
                spec.depthTest = overlay.value("depthTest", spec.depthTest);
            }
            if (!overlay.is_boolean() || overlay.get<bool>())
                job.overlay = spec;
        }

        if (j.contains("checks")) {
            auto& checks = j["checks"];
            CheckKeys(checks, { "grounded", "penetration", "logErrors", "invalidTransforms", "interval" }, "checks");
            if (checks.contains("grounded"))
                job.checks.grounded = ParseGrounded(checks["grounded"]);
            if (checks.contains("penetration"))
                job.checks.penetration = ParsePenetration(checks["penetration"]);
            if (checks.contains("logErrors") && checks["logErrors"].is_object()) {
                CheckKeys(checks["logErrors"], { "ignore" }, "checks.logErrors");
                job.checks.logIgnore = ToStringList(checks["logErrors"].value("ignore", json::array()));
            }
            else {
                job.checks.logErrors = checks.value("logErrors", job.checks.logErrors);
            }
            job.checks.invalidTransforms = checks.value("invalidTransforms", job.checks.invalidTransforms);
            job.checks.interval = checks.value("interval", job.checks.interval);
        }

        if (job.play && job.duration <= 0.0f)
            throw std::runtime_error("A job with play = true needs a positive duration");
        if (!job.play && (!job.timeline.empty() || !job.strips.empty() || !job.scripts.empty()))
            throw std::runtime_error("timeline, strips and scripts require play = true");
        if (job.play && job.edit)
            throw std::runtime_error(
                "edit and play can't be combined, scripts would create runtime entities " "that end up in the saved scene. Run a second job on the edited scene instead");

        // Without any explicit work, take a picture with the scene camera
        if (job.shots.empty() && !job.play && !job.inspect && job.queries.empty() && !job.edit) {
            job.shots.push_back(ShotSpec { .name = "frame" });
        }

        return job;

    }

    Job ParseCommandLine(const std::vector<std::string>& args) {

        json j = json::object();
        fs::path baseDirectory = GetLaunchDirectory();

        json cliShot = json::object();
        json cliCamera = json::object();
        bool validation = false;

        // Skip the executable path
        for (size_t i = 1; i < args.size(); i++) {
            const auto& arg = args[i];

            auto next = [&]() -> std::string {

                if (i + 1 >= args.size())
                    throw std::runtime_error(arg + " expects a value");
                return args[++i];

            };
            auto hasValue = [&]() {

                return i + 1 < args.size() && !args[i + 1].starts_with("--");

            };

            if (arg == "--help" || arg == "-h") {
                throw std::runtime_error(GetUsage());
            }
            else if (arg == "--job" || (!arg.starts_with("--") && arg.ends_with(".json"))) {
                auto path = ResolveUserPath(arg == "--job" ? next() : arg);
                std::ifstream stream(path);
                if (!stream.is_open())
                    throw std::runtime_error("Couldn't open job file " + path.string());

                json fileJob;
                try {
                    fileJob = json::parse(stream, nullptr, true, true);
                }
                catch (const json::exception& e) {
                    throw std::runtime_error("Couldn't parse job file " + path.string() + ": " + e.what());
                }

                // Values given on the command line before the job file are kept
                fileJob.merge_patch(j);
                j = fileJob;
                if (!j.contains("name"))
                    j["name"] = path.stem().string();
                baseDirectory = path.parent_path();
            }
            else if (arg == "--scene")
                j["scene"] = next();
            else if (arg == "--out") {
                j["out"] = ResolveUserPath(next()).string();
            }
            else if (arg == "--name")
                j["name"] = next();
            else if (arg == "--resolution") {
                auto value = next();
                auto parts = SplitList(value, 'x');
                if (parts.size() != 2)
                    throw std::runtime_error("--resolution expects WIDTHxHEIGHT");
                j["resolution"] = json::array({ std::stoi(parts[0]), std::stoi(parts[1]) });
            }
            else if (arg == "--settle")
                j["settleFrames"] = std::stoi(next());
            else if (arg == "--timeout")
                j["loadTimeout"] = std::stof(next());
            else if (arg == "--show")
                j["showWindow"] = true;
            else if (arg == "--hidden")
                j["showWindow"] = false;
            else if (arg == "--overlay")
                j["overlay"] = true;
            else if (arg == "--validation")
                validation = true;
            else if (arg == "--inspect")
                j["inspect"] = hasValue() ? json(next()) : json(true);
            else if (arg == "--play") {
                j["play"] = true;
                j["duration"] = std::stof(next());
            }
            else if (arg == "--camera") {
                cliCamera["position"] = ParseVec3Argument(next(), arg);
            }
            else if (arg == "--look-at")
                cliCamera["lookAt"] = ParseVec3Argument(next(), arg);
            else if (arg == "--yaw")
                cliCamera["yaw"] = std::stof(next());
            else if (arg == "--pitch")
                cliCamera["pitch"] = std::stof(next());
            else if (arg == "--fov")
                cliCamera["fov"] = std::stof(next());
            else if (arg == "--focus")
                cliCamera["entity"] = next();
            else if (arg == "--distance")
                cliCamera["distance"] = std::stof(next());
            else if (arg == "--overview")
                cliCamera["mode"] = "overview";
            else if (arg == "--views")
                cliShot["views"] = SplitList(next());
            else if (arg == "--passes")
                cliShot["passes"] = SplitList(next());
            else if (arg == "--shot")
                cliShot["name"] = next();
            else if (arg == "--set") {
                // --set fog.density=0.02, the value is parsed as JSON if possible
                auto assignment = next();
                auto equals = assignment.find('=');
                if (equals == std::string::npos)
                    throw std::runtime_error("--set expects path=value");

                auto path = assignment.substr(0, equals);
                auto valueText = assignment.substr(equals + 1);
                json value;
                try {
                    value = json::parse(valueText);
                }
                catch (const json::exception&) {
                    value = valueText;
                }

                json* node = &j["settings"];
                for (const auto& key : SplitList(path, '.'))
                    node = &(*node)[key];
                *node = value;
            }
            else {
                throw std::runtime_error("Unknown argument '" + arg + "'\n\n" + GetUsage());
            }
        }

        if (!cliCamera.empty())
            cliShot["camera"] = cliCamera;
        if (!cliShot.empty()) {
            if (!j.contains("shots"))
                j["shots"] = json::array();
            j["shots"].push_back(cliShot);
        }

        if (!j.contains("scene"))
            throw std::runtime_error("No scene given\n\n" + GetUsage());

        auto job = ParseJob(j, baseDirectory);
        job.validation = validation;
        return job;

    }

    std::string GetUsage() {

        return R"(Usage:
  AtlasEngineCapture <job.json> [options]
  AtlasEngineCapture --scene <scene> [options]
  AtlasEngineCapture --compose <out.png> [--title <text>] [--columns <n>] <label>=<image.png> ...

Options:
  --job <file>            Job file (see src/capture/README.md for the format)
  --scene <path>          Scene, relative to data/ or the launch directory
  --out <dir>             Output directory (default: bin/capture/<name>)
  --name <name>           Job name
  --resolution <WxH>      Render resolution (default 1280x720)
  --settle <frames>       Frames rendered before each capture (default 60)
  --timeout <seconds>     Maximum time to wait for the scene to load
  --show                  Show the frames and the output in a window (default)
  --hidden                Render in a hidden window and play as fast as possible
  --overlay               Draw debug lines: players, tracked paths and query hits
  --validation            Enable all Vulkan validation layer features and write validation.json (slow)
  --inspect [file]        Write a scene summary (default scene.json)
  --play <seconds>        Play the scene for the given time
  --set <path=value>      Override a scene setting, e.g. fog.density=0.02
Shot options (create one additional shot):
  --shot <name>           Name of the shot
  --camera <x,y,z>        Manual camera position
  --look-at <x,y,z>       Manual camera target
  --yaw/--pitch <deg>     Manual camera rotation
  --fov <deg>             Field of view
  --focus <pattern>       Frame entities matching the pattern (* and ? wildcards)
  --distance <m>          Focus distance (default: derived from the bounds)
  --views <list>          Focus views: front,right,back,left,top,iso,low,lowSide
  --overview              Frame the whole scene
  --passes <list>         Additional pass images: albedo,normal,geometryNormal,roughness,
                          emissive,depth,velocity,stencil,reflection,volumetric,clouds,sss,gi

Exit codes: 0 success, 1 check/expectation failures, 2 fatal error, 3 crash.
)";

    }

}
