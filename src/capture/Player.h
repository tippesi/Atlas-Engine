#pragma once

#include "Timeline.h"
#include "LuaJob.h"
#include "Checks.h"
#include "Report.h"

#include <functional>

namespace Atlas::Capture {

    // Plays the timeline of a job on a scene: input, mouse, Lua, look, teleport, animations, logs, expectations, pauses
    // and the conditions of events. The capture app and HostedJob use it, captures and resetScripts are left to them
    class Player {

    public:
        enum class EventResult {
            // The next event runs
            Done,
            // The event runs again in the next frame, the remaining events of this frame wait for it
            Defer,
            // The event is done, the owner ends the frame without running more events
            Suspend
        };

        using OwnerEvent = std::function<EventResult(const EventSpec& event)>;

        // Prepares the timeline, see Timeline::Reset
        void Reset(const Job& job);

        // Attaches the scripts of the job, prepares its Lua in the state (the scene's by default) and starts the
        // timeline. The window receives the mouse events
        void Start(const Job& job, Scene::Scene& scene, std::vector<EventSpec> events, Report& report,
            sol::state* luaState, uint32_t windowId);

        void Stop();

        // Releases input, checks the conditions and runs the due events of this frame. Returns false if an event of
        // the owner suspended the frame
        bool Update(const OwnerEvent& ownerEvent);

        // Conditional events that never happened are failures unless they are optional
        void ReportMissedEvents(CheckRunner& checks);

        // Evaluates a condition or an expectation, Lua ones need a started player
        json Expect(const json& expectation, bool& passed);

        Timeline timeline;
        LuaJob lua;

    private:
        void Run(const InputEvent& input);

        void Run(const LookEvent& look);

        void Run(const TeleportEvent& teleport);

        void Run(const AnimationEvent& animation);

        void Run(const LogEvent& log);

        void Run(const ExpectEvent& expect);

        void Run(const PauseEvent& pause);

        void Run(const EndEvent&);

        void Run(const LuaEvent& lua);

        void SetLookDirection(Scene::Entity entity, std::optional<float> yaw, std::optional<float> pitch);

        Scene::Scene* scene = nullptr;
        Report* report = nullptr;

    };

}
