#pragma once

#include "Profiler.h"

#include <deque>
#include <mutex>

namespace Atlas {

    /**
     * Keeps the CPU profile of frames that take much longer than the frames before them. Normal frames only cost a
     * comparison, the profile is copied in the frames that are recorded.
     */
    class HitchRecorder {

    public:
        struct Hitch {
            // Seconds since the start of the application
            float time = 0.0f;
            float frameTime = 0.0f;
            float averageFrameTime = 0.0f;

            // The CPU profile of the frame, all threads
            std::vector<Profiler::ThreadData> threads;
        };

        /**
         * Checks the last frame after the CPU profiler collected it. Is called by the engine.
         * @param frameTime The time of the last frame in milliseconds
         */
        static void Update(float frameTime);

        /**
         * Returns the recorded hitches, the latest last
         */
        static std::vector<Hitch> GetHitches();

        static void Clear();

        // A frame is a hitch if it takes longer than this and longer than the average times the relative threshold
        static float minimumFrameTime;
        static float relativeThreshold;

        // Logs every hitch with the scopes that took at least this share of it
        static bool log;
        static float logMinimumShare;

        static bool enable;

    private:
        static void LogHitch(const Hitch& hitch);

        static constexpr size_t maxHitches = 32;

        static std::mutex mutex;
        static std::deque<Hitch> hitches;
        static float averageFrameTime;

    };

}
