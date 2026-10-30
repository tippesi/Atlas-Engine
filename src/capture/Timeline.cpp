#include "Timeline.h"
#include "Checks.h"

#include "events/EventManager.h"

#include <SDL3/SDL.h>

#include <algorithm>

namespace Atlas::Capture {

    // Accumulated fixed time steps don't exactly hit the times of the timeline
    static constexpr float timeTolerance = 1e-4f;

    void Timeline::Reset(const std::vector<EventSpec>& jobEvents) {

        time = 0.0f;
        events.clear();
        nextEvent = 0;
        ended = false;
        triggers.clear();
        pendingReleases.clear();
        DetachController();

        auto usesController = [](const EventSpec& event) {
            auto input = std::get_if<InputEvent>(&event.action);
            return input && input->device != InputEvent::Device::Key;
        };
        if (std::any_of(jobEvents.begin(), jobEvents.end(), usesController))
            AttachController();

    }

    size_t Timeline::Start(std::vector<EventSpec> timedEvents, float duration) {

        time = 0.0f;
        nextEvent = 0;
        ended = false;

        events.clear();
        triggers.clear();
        for (auto& event : timedEvents) {
            if (event.when)
                triggers.push_back({ .event = event });
            else
                events.push_back(event);
        }

        std::stable_sort(events.begin(), events.end(), [](const EventSpec& a, const EventSpec& b) {

            return a.time < b.time;

        });

        auto beyond = std::remove_if(events.begin(), events.end(), [&](const EventSpec& event) {

            return event.time > duration + timeTolerance;

        });
        auto dropped = size_t(std::distance(beyond, events.end()));
        events.erase(beyond, events.end());
        return dropped;

    }

    bool Timeline::IsDue(float eventTime) const {

        return eventTime <= time + timeTolerance;

    }

    void Timeline::Update(Scene::Scene& scene, const std::function<void(const std::string&)>& print) {

        for (auto it = pendingReleases.begin(); it != pendingReleases.end();) {
            if (IsDue(it->first)) {
                SetInput(it->second, false);
                it = pendingReleases.erase(it);
            }
            else {
                ++it;
            }
        }

        std::vector<EventSpec> due;
        for (auto& trigger : triggers) {
            const auto& when = *trigger.event.when;
            if (!trigger.metTime && IsDue(trigger.event.time)) {
                bool passed = trigger.logMatched;
                if (!when.contains("log"))
                    EvaluateExpectation(scene, when, passed);
                if (passed) {
                    trigger.metTime = time;
                    print(FormatTime(time) + " condition met: " + when.dump());
                }
            }

            if (trigger.metTime && !trigger.fired && IsDue(*trigger.metTime + trigger.event.delay)) {
                trigger.fired = true;
                auto event = trigger.event;
                event.time = time;
                due.push_back(event);
            }
        }

        // Due events run in this frame before the timed events of the frame
        events.insert(events.begin() + nextEvent, due.begin(), due.end());

    }

    void Timeline::MatchLog(const std::string& message) {

        for (auto& trigger : triggers) {
            if (trigger.logMatched || !IsDue(trigger.event.time) || !trigger.event.when->contains("log"))
                continue;
            auto pattern = (*trigger.event.when)["log"].get<std::string>();
            if (pattern.find_first_of("*?") == std::string::npos)
                pattern = "*" + pattern + "*";
            trigger.logMatched = Helper::String::MatchWildcard(pattern, message);
        }

    }

    const EventSpec* Timeline::GetDueEvent() const {

        if (nextEvent >= events.size() || !IsDue(events[nextEvent].time))
            return nullptr;

        return &events[nextEvent];

    }

    void Timeline::PopEvent() {

        nextEvent = std::min(nextEvent + 1, events.size());

    }

    void Timeline::End() {

        ended = true;
        nextEvent = events.size();

    }

    bool Timeline::IsEnded() const {

        return ended;

    }

    bool Timeline::IsFinished(float duration) const {

        return (ended || IsDue(duration)) && nextEvent >= events.size();

    }

    void Timeline::Fire(const InputEvent& input) {

        if (input.device == InputEvent::Device::Axis) {
            SetInput(input, true);
            return;
        }

        if (input.action == "down" || input.action == "press")
            SetInput(input, true);
        if (input.action == "up")
            SetInput(input, false);
        if (input.action == "press")
            pendingReleases.emplace_back(time + input.duration, input);

    }

    std::vector<EventSpec> Timeline::GetMissedEvents() const {

        std::vector<EventSpec> missed;
        for (const auto& trigger : triggers)
            if (!trigger.fired)
                missed.push_back(trigger.event);

        return missed;

    }

    json Timeline::GetTriggerReport() const {

        if (triggers.empty())
            return json();

        json report = json::array();
        for (const auto& trigger : triggers)
            report.push_back({ { "event", trigger.event.data }, { "fired", trigger.fired },
                { "conditionMetAt", trigger.metTime ? json(*trigger.metTime) : json() } });

        return report;

    }

    size_t Timeline::GetTimedEventCount() const {

        return events.size();

    }

    size_t Timeline::GetTriggerCount() const {

        return triggers.size();

    }

    void Timeline::SetInput(const InputEvent& input, bool down) {

        if (input.device == InputEvent::Device::Key) {
            auto keyCode = SDL_GetKeyFromName(input.name.c_str());
            if (keyCode == SDLK_UNKNOWN)
                throw std::runtime_error("Unknown key '" + input.name + "', use SDL key names like E, Space, Left Shift, Up");

            SDL_KeyboardEvent event = {};
            event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
            event.down = down;
            event.repeat = false;
            event.key = keyCode;
            event.scancode = SDL_GetScancodeFromKey(keyCode, nullptr);

            Events::EventManager::KeyboardEventDelegate.Fire(Events::KeyboardEvent(event));
            return;
        }

        AttachController();
        if (input.device == InputEvent::Device::Button) {
            auto index = SDL_GetGamepadButtonFromString(input.name.c_str());
            if (index == SDL_GAMEPAD_BUTTON_INVALID)
                throw std::runtime_error("Unknown controller button '" + input.name + "', use SDL names like a, b, x, y, leftshoulder, dpup, start");
            SDL_SetJoystickVirtualButton(controller, index, down);
            return;
        }

        auto index = SDL_GetGamepadAxisFromString(input.name.c_str());
        if (index == SDL_GAMEPAD_AXIS_INVALID)
            throw std::runtime_error("Unknown controller axis '" + input.name + "', use leftx, lefty, rightx, righty, lefttrigger or righttrigger");
        SDL_SetJoystickVirtualAxis(controller, index, Sint16(glm::clamp(input.value, -1.0f, 1.0f) * 32767.0f));

    }

    void Timeline::AttachController() {

        if (controller)
            return;

        // SDL reports the virtual controller like a connected one, the engine receives its events
        SDL_VirtualJoystickDesc desc;
        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.name = "Capture controller";
        controller = SDL_OpenJoystick(SDL_AttachVirtualJoystick(&desc));
        if (!controller)
            throw std::runtime_error("Couldn't attach a virtual controller: " + std::string(SDL_GetError()));

    }

    void Timeline::DetachController() {

        if (!controller)
            return;

        auto instance = SDL_GetJoystickID(controller);
        SDL_CloseJoystick(controller);
        controller = nullptr;
        SDL_DetachVirtualJoystick(instance);

    }

}
