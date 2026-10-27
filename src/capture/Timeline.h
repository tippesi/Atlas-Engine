#pragma once

#include "Job.h"

#include "scene/Scene.h"

#include <SDL_joystick.h>

#include <functional>

namespace Atlas::Capture {

    // The events of a playing job: timed events in the order of their time, conditional events that wait
    // for their condition and the input the events simulate
    class Timeline {

    public:
        struct Trigger {

            EventSpec event;
            bool logMatched = false;
            std::optional<float> metTime;
            bool fired = false;

        };

        // Prepares the timeline of a job. A virtual controller is attached right away, SDL reports it a few
        // frames later and earlier events would be lost
        void Reset(const std::vector<EventSpec>& jobEvents);

        // Starts playing at time 0, events with a condition become triggers. Returns the number of
        // events after the duration, which are dropped
        size_t Start(std::vector<EventSpec> timedEvents, float duration);

        // Whether a time is reached, tolerates the rounding of the fixed time steps
        bool IsDue(float time) const;

        // Releases the keys and buttons whose press is over and queues the events whose condition is met
        void Update(Scene::Scene& scene, const std::function<void(const std::string&)>& print);

        // Log conditions: a text without wildcards matches when the message contains it
        void MatchLog(const std::string& message);

        // The next event that is due in this frame, nullptr if there is none
        const EventSpec* GetDueEvent() const;

        void PopEvent();

        // Skips the remaining timed events, conditional events that didn't happen yet still count as missed
        void End();

        bool IsEnded() const;

        // All events ran and the duration (or an end event) is reached
        bool IsFinished(float duration) const;

        // Keys and buttons go down, up or are pressed and released after the duration, axes get their value
        void Fire(const InputEvent& input);

        // Conditional events that never fired
        std::vector<EventSpec> GetMissedEvents() const;

        // When the conditions were met and whether their events fired, null without conditional events
        json GetTriggerReport() const;

        size_t GetTimedEventCount() const;

        size_t GetTriggerCount() const;

        float time = 0.0f;

    private:
        void SetInput(const InputEvent& input, bool down);

        void AttachController();

        void DetachController();

        std::vector<EventSpec> events;
        size_t nextEvent = 0;
        bool ended = false;
        std::vector<Trigger> triggers;

        // Pressed keys and buttons with the time they are released
        std::vector<std::pair<float, InputEvent>> pendingReleases;
        SDL_Joystick* controller = nullptr;

    };

}
