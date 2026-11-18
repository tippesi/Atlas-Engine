#pragma once

#include "Profiler.h"

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>

namespace Atlas {

    /**
     * Records frames that take much longer than the frames before them, on the CPU and on the GPU, with their profile.
     * Checking a frame costs a few comparisons, the profiles are copied in the frames that are recorded.
     */
    class HitchRecorder {

    public:
        enum class Source {
            CPU = 0,
            GPU
        };

        /**
         * The time a detector compares.
         * FRAME_TIME: Time between two frames on the main thread (CPU)
         * MAIN_THREAD: Time of the profiler queries of the main thread (CPU)
         * RENDER_THREAD: Time of the profiler queries of the render thread (CPU)
         * GPU_BUSY: Time of the queries of the busiest GPU profiler thread (GPU)
         * GPU_FRAME: Time from the first to the last GPU timestamp of the frame, including idle gaps (GPU)
         */
        enum class Metric {
            FRAME_TIME = 0,
            MAIN_THREAD,
            RENDER_THREAD,
            GPU_BUSY,
            GPU_FRAME
        };

        /**
         * When a frame counts as a hitch.
         * ABSOLUTE: Longer than the minimum time
         * RELATIVE: Longer than the average times the relative threshold
         * ABSOLUTE_AND_RELATIVE: Both of the above
         */
        enum class Trigger {
            ABSOLUTE = 0,
            RELATIVE,
            ABSOLUTE_AND_RELATIVE
        };

        struct Detector {
            bool enable = true;

            Metric metric = Metric::FRAME_TIME;
            Trigger trigger = Trigger::ABSOLUTE_AND_RELATIVE;

            // Milliseconds
            float minimumTime = 40.0f;
            // Greater than 1, hitches count into the average up to this factor such that it follows lasting changes
            float relativeThreshold = 2.0f;
        };

        struct Settings {
            bool enable = true;

            Detector cpu;
            // Needs the GPU profiler
            Detector gpu = { .enable = false, .metric = Metric::GPU_BUSY, .minimumTime = 20.0f };

            uint32_t maxHitches = 128;

            // Logs every hitch with the scopes that took at least this share of it
            bool log = false;
            float logMinimumShare = 0.1f;
        };

        // A query which took much longer in the hitch than on average
        struct Cause {
            std::string thread;
            // Names from the root query to the cause, separated by " > "
            std::string path;

            // Milliseconds
            float time = 0.0f;
            float averageTime = 0.0f;
        };

        struct Hitch {
            // Increases with every recorded hitch
            uint64_t id = 0;
            Source source = Source::CPU;
            Metric metric = Metric::FRAME_TIME;

            // Seconds since the start of the application. GPU hitches are found when their frame is read back,
            // a few frames later
            float time = 0.0f;
            // Milliseconds of the metric
            float frameTime = 0.0f;
            float averageFrameTime = 0.0f;

            // The profile of the frame from the profiler of the source, all threads, slowest queries first
            std::vector<Profiler::ThreadData> threads;
            // Largest excess first
            std::vector<Cause> causes;
        };

        // The causes of several hitches merged by thread and path
        struct CauseSummary {
            std::string thread;
            std::string path;

            uint32_t hitches = 0;
            // Milliseconds over the average, summed and the largest one
            float totalExcess = 0.0f;
            float maxExcess = 0.0f;
        };

        /**
         * Checks the last frame on the CPU after the CPU profiler collected it. Is called by the engine.
         * @param frameTime The time of the last frame in milliseconds
         */
        static void Update(float frameTime);

        /**
         * Checks a frame on the GPU after the GPU profiler read it back. Is called by the GPU profiler.
         * @param busyTime Milliseconds of the busiest GPU profiler thread
         * @param frameTime Milliseconds from the first to the last timestamp
         * @param getThreads Returns the queries of the frame, only called for a hitch
         */
        static void UpdateGPU(float busyTime, float frameTime,
            const std::function<std::vector<Profiler::ThreadData>()>& getThreads);

        /**
         * Returns the recorded hitches, the latest last. Recorded hitches don't change and are shared with the copies
         */
        static std::vector<Ref<const Hitch>> GetHitches();

        /**
         * Changes with every recorded hitch and every clear, copies of the hitches only need an update then
         */
        static uint64_t GetRevision();

        /**
         * Merges the causes of hitches, the largest total excess first
         */
        static std::vector<CauseSummary> Evaluate(const std::vector<Ref<const Hitch>>& hitches);

        // Also restarts the averages
        static void Clear();

        static const char* GetMetricName(Metric metric);

        // Identifier of the metric for files, e.g. frameTime
        static const char* GetMetricId(Metric metric);

        // The settings can be changed from any thread, the detectors use a copy per frame
        static Settings GetSettings();

        static void SetSettings(const Settings& settings);

    private:
        struct DetectorState {
            float averageFrameTime = 0.0f;
            // The average restarts when the settings version changes
            uint64_t settingsVersion = 0;
        };

        static bool IsHitch(const Detector& detector, DetectorState& state, uint64_t version, float frameTime);

        static void Record(Hitch&& hitch, const std::vector<Profiler::ThreadData>& averageThreads, const Settings& settings);

        static void FindCauses(Hitch& hitch, const std::vector<Profiler::ThreadData>& averageThreads);

        static void LogHitch(const Hitch& hitch, float minimumShare);

        static void TrimHitches(uint32_t maxHitches);

        // Frames of the averages the causes are compared with, the hitch is one of them
        static constexpr uint32_t averageFrames = 64;

        static std::mutex mutex;
        static Settings settings;
        static uint64_t settingsVersion;
        static std::deque<Ref<const Hitch>> hitches;
        static uint64_t nextId;
        static std::atomic_uint64_t revision;

        static DetectorState cpuState;
        static DetectorState gpuState;

    };

}
