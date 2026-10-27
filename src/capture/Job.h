#pragma once

#include "Common.h"

#include <optional>
#include <variant>

namespace Atlas::Capture {

    struct CameraSpec {

        // scene, manual, focus, follow or overview
        std::string mode = "scene";

        vec3 position = vec3(0.0f);
        std::optional<vec3> lookAt;
        float yaw = 0.0f;
        float pitch = 0.0f;

        std::string entity;
        // Empty means front for focus cameras and iso for overview cameras
        std::string view;
        std::optional<float> azimuth;
        std::optional<float> elevation;
        // Rotate the view directions with the yaw of the focused entity
        bool local = false;
        // 0 means the distance is derived from the bounds of the focused entities
        float distance = 0.0f;
        float padding = 1.25f;
        // Looks for a free line of sight if geometry would block the view of a focus camera
        bool avoidOcclusion = true;
        // An observer focus camera is framed once when playing starts unless it is dynamic
        bool dynamic = false;
        // Offset of the follow camera relative to the followed entity (in its local space if local = true)
        vec3 offset = vec3(0.0f, 2.0f, -6.0f);

        // Manual cameras: the heights of the position and the target are relative to the ground below them
        bool groundRelative = false;

        float fieldOfView = 60.0f;
        float nearPlane = 0.1f;
        float farPlane = 2000.0f;

    };

    struct ShotSpec {

        std::string name;
        CameraSpec camera;
        // Multiple views of a focus camera, e.g. front, right, back, left, top, iso
        std::vector<std::string> views;
        std::vector<std::string> passes;
        int32_t settleFrames = -1;

    };

    struct VariantSpec {

        std::string name;
        json settings = json::object();
        // Overrides the render scale of the job
        std::optional<float> renderScale;

    };

    struct CaptureEvent {

        std::string name = "capture";
        // Default: <name>_<time>s.png
        std::optional<std::string> file;
        // Sheet the capture is added to
        std::string group;
        // Default: the time of the capture
        std::optional<std::string> label;
        std::optional<CameraSpec> camera;
        // Uses the camera of the strip of the group
        bool stripCamera = false;
        // Settles on frozen frames like a capture with its own camera
        bool still = false;
        std::optional<int32_t> settleFrames;
        std::vector<std::string> passes;

    };

    // Simulated input with SDL names: keys and controller buttons go down, up or are pressed for a duration,
    // controller axes keep their value until it changes
    struct InputEvent {

        enum class Device {
            Key,
            Button,
            Axis
        };

        Device device = Device::Key;
        std::string name;
        // down, up or press
        std::string action = "press";
        float duration = 0.0f;
        float value = 0.0f;

    };

    struct LookEvent {

        std::string entity;
        std::optional<float> yaw;
        std::optional<float> pitch;

    };

    struct TeleportEvent {

        std::string entity;
        vec3 position = vec3(0.0f);
        std::optional<float> yaw;
        std::optional<float> pitch;

    };

    struct AnimationEvent {

        std::string entity;
        // An animation file or the name of an animation in the model file
        std::string animation;
        std::string model;
        float blendTime = 0.0f;

    };

    struct LogEvent {

        std::string text;

    };

    struct ExpectEvent {

        json expectation;

    };

    struct PauseEvent {

        bool pause = true;

    };

    struct ResetScriptsEvent {};

    // Finishes the job early, the remaining timed events are skipped
    struct EndEvent {};

    using EventAction = std::variant<CaptureEvent, InputEvent, LookEvent, TeleportEvent, AnimationEvent, LogEvent, ExpectEvent,
        PauseEvent, ResetScriptsEvent, EndEvent>;

    struct EventSpec {

        float time = 0.0f;
        // The key of the action in the job, e.g. "capture"
        std::string type;
        EventAction action;
        // The event as written in the job, for the report
        json data;

        // Condition that triggers the event, the time is then the earliest time it is checked
        std::optional<json> when;
        float delay = 0.0f;
        // Conditional events that never happen fail the job unless they are optional
        bool optional = false;

    };

    struct StripSpec {

        std::string name;
        float from = 0.0f;
        float to = 0.0f;
        float every = 0.5f;
        CameraSpec camera;
        // Strips with their own camera are captured as stills, otherwise the observer is used
        bool hasCamera = false;
        int32_t columns = 0;

    };

    // Lua script attached while playing, the scene file is not changed
    struct ScriptSpec {

        std::string path;
        // Entity pattern to attach the script to, a new entity is created if empty
        std::string entity;

    };

    struct GroundedCheckSpec {

        std::vector<std::string> entities;
        // Entities with these meshes (asset path patterns), e.g. every instance of a tree mesh
        std::vector<std::string> meshes;
        float maxGap = 0.15f;
        float maxSink = 0.3f;
        // Optional patterns the surface below has to match (by its or a parent's name), e.g. "*Ground*"
        std::vector<std::string> surfaces;
        // Hierarchies the ground ray passes, e.g. undergrowth around trees
        std::vector<std::string> ignore;

    };

    struct PenetrationCheckSpec {

        std::vector<std::string> entities;
        std::vector<std::string> against;
        // The entity bounds are shrunk by this margin such that resting contact doesn't count
        float margin = 0.05f;
        // Number of additional intruding triangles (relative to the start) that counts as a penetration
        int32_t minTriangles = 1;

    };

    struct ChecksSpec {

        std::optional<GroundedCheckSpec> grounded;
        std::optional<PenetrationCheckSpec> penetration;
        bool logErrors = true;
        // Engine errors matching these patterns don't fail the job, e.g. known missing assets
        std::vector<std::string> logIgnore;
        bool invalidTransforms = true;
        float interval = 0.1f;

    };

    struct OverlaySpec {

        std::vector<std::string> bounds;
        // [{ "position": [x, y, z], "color": [r, g, b], "size": 0.5 }]
        json markers = json::array();
        std::vector<vec3> axes;
        bool players = true;
        bool trails = true;
        bool queries = true;
        bool depthTest = false;

    };

    struct InspectSpec {

        std::string file = "scene.json";
        std::vector<std::string> filter;
        // Entities for which the full serialized component data is written
        std::vector<std::string> detail;

    };

    struct TrackSpec {

        std::vector<std::string> entities;
        float interval = 0.1f;

    };

    struct EditSpec {

        json operations = json::array();
        std::string output;
        bool overwrite = false;
        // Saves the scene even if no entity changed, e.g. to store settings or data migrated while loading
        bool saveUnchanged = false;
        // Applies the operations and runs the checks and shots on the edited scene, but saves nothing
        bool dryRun = false;
        // Queries on the edited scene, written to edit_queries.json
        json queries = json::array();
        // Textures of imported models are stored as compressed KTX2 files ("ktx2") or like their source ("original")
        std::string textureFormat = "original";

    };

    struct Job {

        std::string name = "capture";
        std::string scene;
        fs::path outputDirectory;

        ivec2 resolution = ivec2(1280, 720);
        // Scaling factor of the render target relative to the resolution, upscaled by FSR2 or TAA. Unset keeps the engine default
        std::optional<float> renderScale;
        int32_t settleFrames = 60;
        float loadTimeout = 300.0f;
        float fixedDeltaTime = 1.0f / 60.0f;
        // Shows the frames and the output while running, playback then runs in real time unless realTime is off
        bool showWindow = true;
        bool realTime = true;
        // Renders with the path tracer, which accumulates over the settle frames
        bool pathTrace = false;

        json settings = json::object();
        std::vector<VariantSpec> variants;

        std::optional<InspectSpec> inspect;
        json queries = json::array();
        std::optional<EditSpec> edit;

        std::vector<ShotSpec> shots;

        bool play = false;
        float duration = 0.0f;
        std::vector<EventSpec> timeline;
        std::vector<StripSpec> strips;
        std::optional<CameraSpec> observer;
        std::vector<ScriptSpec> scripts;

        std::optional<TrackSpec> track;
        std::optional<OverlaySpec> overlay;
        ChecksSpec checks;

        // Number of last frames the GPU timings are averaged over, 0 disables the profile
        int32_t profileFrames = 0;

        // Only set by --validation, the engine enables the validation layers before the job is read
        bool validation = false;
        // Only set by --sound, the capture tool is muted otherwise
        bool sound = false;

    };

    // Parses the command line, which either references a job file or describes a simple job
    // Throws std::runtime_error with a readable message on invalid input
    Job ParseCommandLine(const std::vector<std::string>& args);

    Job ParseJob(const json& j, const fs::path& baseDirectory);

    CameraSpec ParseCamera(const json& j);

    std::string GetUsage();

}
