#include "HitchRecorder.h"
#include "CPUProfiler.h"
#include "GPUProfiler.h"
#include "../Clock.h"
#include "../Log.h"

#include <algorithm>
#include <cstdio>

namespace Atlas {

    HitchRecorder::Settings HitchRecorder::settings;
    uint64_t HitchRecorder::settingsVersion = 0;

    std::mutex HitchRecorder::mutex;
    std::deque<Ref<const HitchRecorder::Hitch>> HitchRecorder::hitches;
    uint64_t HitchRecorder::nextId = 0;
    std::atomic_uint64_t HitchRecorder::revision = 0;

    HitchRecorder::DetectorState HitchRecorder::cpuState;
    HitchRecorder::DetectorState HitchRecorder::gpuState;

    void HitchRecorder::Update(float frameTime) {

        Settings currentSettings;
        uint64_t version;
        {
            std::scoped_lock lock(mutex);
            currentSettings = settings;
            version = settingsVersion;
        }

        const auto& detector = currentSettings.cpu;
        if (!currentSettings.enable || !detector.enable)
            return;

        auto metric = detector.metric == Metric::MAIN_THREAD || detector.metric == Metric::RENDER_THREAD ?
            detector.metric : Metric::FRAME_TIME;
        auto metricTime = frameTime;
        if (metric == Metric::MAIN_THREAD)
            metricTime = CPUProfiler::GetThreadTime("Main");
        else if (metric == Metric::RENDER_THREAD)
            metricTime = CPUProfiler::GetThreadTime("Render");

        // A thread without a profile, e.g. without a render thread, has nothing to compare
        if (metricTime <= 0.0f)
            return;

        auto averageFrameTime = cpuState.averageFrameTime;
        if (!IsHitch(detector, cpuState, version, metricTime))
            return;

        Hitch hitch = {
            .source = Source::CPU,
            .metric = metric,
            .time = Clock::Get(),
            .frameTime = metricTime,
            .averageFrameTime = averageFrameTime,
            .threads = CPUProfiler::GetQueries(Profiler::OrderBy::MAX_TIME)
        };

        Record(std::move(hitch), CPUProfiler::GetQueriesAverage(averageFrames), currentSettings);

    }

    void HitchRecorder::UpdateGPU(float busyTime, float frameTime,
        const std::function<std::vector<Profiler::ThreadData>()>& getThreads) {

        Settings currentSettings;
        uint64_t version;
        {
            std::scoped_lock lock(mutex);
            currentSettings = settings;
            version = settingsVersion;
        }

        const auto& detector = currentSettings.gpu;
        if (!currentSettings.enable || !detector.enable)
            return;

        auto metric = detector.metric == Metric::GPU_FRAME ? Metric::GPU_FRAME : Metric::GPU_BUSY;
        auto metricTime = metric == Metric::GPU_FRAME ? frameTime : busyTime;
        if (metricTime <= 0.0f)
            return;

        auto averageFrameTime = gpuState.averageFrameTime;
        if (!IsHitch(detector, gpuState, version, metricTime))
            return;

        Hitch hitch = {
            .source = Source::GPU,
            .metric = metric,
            .time = Clock::Get(),
            .frameTime = metricTime,
            .averageFrameTime = averageFrameTime,
            .threads = getThreads()
        };

        Record(std::move(hitch), GPUProfiler::GetQueriesAverage(averageFrames), currentSettings);

    }

    std::vector<Ref<const HitchRecorder::Hitch>> HitchRecorder::GetHitches() {

        std::scoped_lock lock(mutex);
        return std::vector<Ref<const Hitch>>(hitches.begin(), hitches.end());

    }

    uint64_t HitchRecorder::GetRevision() {

        return revision.load();

    }

    std::vector<HitchRecorder::CauseSummary> HitchRecorder::Evaluate(const std::vector<Ref<const Hitch>>& hitches) {

        std::vector<CauseSummary> summaries;
        for (const auto& hitch : hitches) {
            for (const auto& cause : hitch->causes) {
                // Jobs run on any worker, the same job on different workers is the same cause
                auto thread = cause.thread.starts_with("Worker ") ? std::string_view("Workers") : std::string_view(cause.thread);
                auto iter = std::find_if(summaries.begin(), summaries.end(), [&](const CauseSummary& summary) {
                    return summary.thread == thread && summary.path == cause.path;
                    });
                if (iter == summaries.end()) {
                    summaries.push_back({ .thread = std::string(thread), .path = cause.path });
                    iter = std::prev(summaries.end());
                }

                auto excess = cause.time - cause.averageTime;
                iter->hitches++;
                iter->totalExcess += excess;
                iter->maxExcess = std::max(iter->maxExcess, excess);
            }
        }

        std::sort(summaries.begin(), summaries.end(), [](const CauseSummary& summary0, const CauseSummary& summary1) {
            return summary0.totalExcess > summary1.totalExcess;
            });

        return summaries;

    }

    void HitchRecorder::Clear() {

        std::scoped_lock lock(mutex);
        hitches.clear();
        settingsVersion++;
        revision++;

    }

    const char* HitchRecorder::GetMetricName(Metric metric) {

        switch (metric) {
            case Metric::MAIN_THREAD: return "Main thread";
            case Metric::RENDER_THREAD: return "Render thread";
            case Metric::GPU_BUSY: return "GPU busy";
            case Metric::GPU_FRAME: return "GPU frame";
            default: return "Frame time";
        }

    }

    const char* HitchRecorder::GetMetricId(Metric metric) {

        switch (metric) {
            case Metric::MAIN_THREAD: return "mainThread";
            case Metric::RENDER_THREAD: return "renderThread";
            case Metric::GPU_BUSY: return "gpuBusy";
            case Metric::GPU_FRAME: return "gpuFrame";
            default: return "frameTime";
        }

    }

    HitchRecorder::Settings HitchRecorder::GetSettings() {

        std::scoped_lock lock(mutex);
        return settings;

    }

    void HitchRecorder::SetSettings(const Settings& newSettings) {

        std::scoped_lock lock(mutex);
        // Averages of another metric don't apply
        if (newSettings.cpu.metric != settings.cpu.metric || newSettings.gpu.metric != settings.gpu.metric)
            settingsVersion++;
        settings = newSettings;
        TrimHitches(settings.maxHitches);

    }

    bool HitchRecorder::IsHitch(const Detector& detector, DetectorState& state, uint64_t version, float frameTime) {

        if (state.settingsVersion != version || state.averageFrameTime <= 0.0f) {
            state.settingsVersion = version;
            state.averageFrameTime = frameTime;
            return false;
        }

        auto threshold = state.averageFrameTime * detector.relativeThreshold;
        auto aboveMinimum = frameTime >= detector.minimumTime;
        auto aboveAverage = frameTime >= threshold;

        bool isHitch = false;
        switch (detector.trigger) {
            case Trigger::ABSOLUTE: isHitch = aboveMinimum; break;
            case Trigger::RELATIVE: isHitch = aboveAverage; break;
            default: isHitch = aboveMinimum && aboveAverage; break;
        }

        // Single hitches barely move the average, lasting changes pull it along
        auto sample = isHitch ? std::min(frameTime, threshold) : frameTime;
        state.averageFrameTime += (sample - state.averageFrameTime) * 0.05f;

        return isHitch;

    }

    void HitchRecorder::Record(Hitch&& hitch, const std::vector<Profiler::ThreadData>& averageThreads, const Settings& currentSettings) {

        FindCauses(hitch, averageThreads);

        if (currentSettings.log)
            LogHitch(hitch, currentSettings.logMinimumShare);

        std::scoped_lock lock(mutex);
        hitch.id = nextId++;
        hitches.push_back(CreateRef<const Hitch>(std::move(hitch)));
        TrimHitches(settings.maxHitches);
        revision++;

    }

    void HitchRecorder::FindCauses(Hitch& hitch, const std::vector<Profiler::ThreadData>& averageThreads) {

        // A cause explains a noticeable part of the hitch. It is the deepest query that does, a parent only when
        // none of its children takes much longer on its own
        auto minimumExcess = std::max(hitch.frameTime - hitch.averageFrameTime, 1.0f) * 0.2;

        auto findQuery = [](const std::vector<Profiler::Query>* queries, const std::string& name) -> const Profiler::Query* {
            if (!queries)
                return nullptr;
            auto iter = std::find_if(queries->begin(), queries->end(),
                [&](const Profiler::Query& query) { return query.name == name; });
            return iter != queries->end() ? &*iter : nullptr;
            };

        // The averages contain the hitch, it is taken out again
        auto averageTimeOf = [](const Profiler::Query& query, const Profiler::Query* averageQuery) {
            if (!averageQuery)
                return 0.0;
            auto averageTime = (double(averageQuery->elapsedTime) * double(averageFrames) - double(query.elapsedTime)) /
                double(averageFrames - 1);
            return std::max(averageTime, 0.0) / 1000000.0;
            };

        auto excessOf = [&](const Profiler::Query& query, const std::vector<Profiler::Query>* averageQueries) {
            return double(query.elapsedTime) / 1000000.0 - averageTimeOf(query, findQuery(averageQueries, query.name));
            };

        std::function<void(const std::string&, const Profiler::Query&, const Profiler::Query*, const std::string&)> addCauses;
        addCauses = [&](const std::string& thread, const Profiler::Query& query, const Profiler::Query* averageQuery,
            const std::string& parentPath) {
            auto path = parentPath.empty() ? query.name : parentPath + " > " + query.name;
            auto averageChildren = averageQuery ? &averageQuery->children : nullptr;

            bool childIsCause = false;
            for (const auto& child : query.children) {
                if (excessOf(child, averageChildren) < minimumExcess)
                    continue;
                addCauses(thread, child, findQuery(averageChildren, child.name), path);
                childIsCause = true;
            }

            if (childIsCause)
                return;

            hitch.causes.push_back({
                .thread = thread,
                .path = path,
                .time = float(double(query.elapsedTime) / 1000000.0),
                .averageTime = float(averageTimeOf(query, averageQuery))
                });
            };

        for (const auto& thread : hitch.threads) {
            auto averageThread = std::find_if(averageThreads.begin(), averageThreads.end(),
                [&](const Profiler::ThreadData& data) { return data.name == thread.name; });
            auto averageQueries = averageThread != averageThreads.end() ? &averageThread->queries : nullptr;

            for (const auto& query : thread.queries) {
                if (excessOf(query, averageQueries) < minimumExcess)
                    continue;
                addCauses(thread.name, query, findQuery(averageQueries, query.name), "");
            }
        }

        std::sort(hitch.causes.begin(), hitch.causes.end(), [](const Cause& cause0, const Cause& cause1) {
            return cause0.time - cause0.averageTime > cause1.time - cause1.averageTime;
            });

    }

    void HitchRecorder::LogHitch(const Hitch& hitch, float minimumShare) {

        char line[256];
        std::snprintf(line, sizeof(line), "%s hitch: %.1f ms %s, average %.1f ms, at %.1f s",
            hitch.source == Source::GPU ? "GPU" : "CPU", hitch.frameTime, GetMetricName(hitch.metric),
            hitch.averageFrameTime, hitch.time);
        std::string text = line;

        // Only the scopes that took a noticeable part of the frame, the slowest first
        auto minimumTime = double(hitch.frameTime) * double(minimumShare) * 1000000.0;
        std::function<void(const Profiler::Query&, int32_t)> addQuery = [&](const Profiler::Query& query, int32_t depth) {
            if (double(query.elapsedTime) < minimumTime)
                return;

            std::snprintf(line, sizeof(line), "\n%*s%.1f ms %s", depth * 2, "", double(query.elapsedTime) / 1000000.0,
                query.name.c_str());
            text += line;
            if (query.calls > 1)
                text += " (" + std::to_string(query.calls) + " calls)";
            for (const auto& child : query.children)
                addQuery(child, depth + 1);
            };

        for (const auto& thread : hitch.threads) {
            double threadTime = 0.0;
            for (const auto& query : thread.queries)
                threadTime += double(query.elapsedTime);
            if (threadTime < minimumTime)
                continue;

            std::snprintf(line, sizeof(line), "\n  %s %.1f ms", thread.name.c_str(), threadTime / 1000000.0);
            text += line;
            for (const auto& query : thread.queries)
                addQuery(query, 2);
        }

        for (const auto& cause : hitch.causes) {
            std::snprintf(line, sizeof(line), "\n  Cause: %s %s %.1f ms (average %.1f ms)", cause.thread.c_str(),
                cause.path.c_str(), cause.time, cause.averageTime);
            text += line;
        }

        Log::Warning(text);

    }

    void HitchRecorder::TrimHitches(uint32_t maxHitches) {

        auto trimmed = false;
        while (hitches.size() > std::max(maxHitches, 1u)) {
            hitches.pop_front();
            trimmed = true;
        }
        if (trimmed)
            revision++;

    }

}
