#include "HitchRecorder.h"
#include "CPUProfiler.h"
#include "../Clock.h"
#include "../Log.h"

#include <cstdio>
#include <functional>

namespace Atlas {

    float HitchRecorder::minimumFrameTime = 40.0f;
    float HitchRecorder::relativeThreshold = 2.0f;

    bool HitchRecorder::log = false;
    float HitchRecorder::logMinimumShare = 0.1f;

    bool HitchRecorder::enable = true;

    std::mutex HitchRecorder::mutex;
    std::deque<HitchRecorder::Hitch> HitchRecorder::hitches;
    float HitchRecorder::averageFrameTime = 0.0f;

    void HitchRecorder::Update(float frameTime) {

        if (!enable)
            return;

        if (averageFrameTime <= 0.0f)
            averageFrameTime = frameTime;

        if (frameTime < minimumFrameTime || frameTime < averageFrameTime * relativeThreshold) {
            // Hitches don't count into the average, it describes the frames around them
            averageFrameTime += (frameTime - averageFrameTime) * 0.05f;
            return;
        }

        Hitch hitch = {
            .time = Clock::Get(),
            .frameTime = frameTime,
            .averageFrameTime = averageFrameTime,
            .threads = CPUProfiler::GetQueries(Profiler::OrderBy::MAX_TIME)
        };

        if (log)
            LogHitch(hitch);

        std::scoped_lock lock(mutex);
        hitches.push_back(std::move(hitch));
        if (hitches.size() > maxHitches)
            hitches.pop_front();

    }

    std::vector<HitchRecorder::Hitch> HitchRecorder::GetHitches() {

        std::scoped_lock lock(mutex);
        return std::vector<Hitch>(hitches.begin(), hitches.end());

    }

    void HitchRecorder::Clear() {

        std::scoped_lock lock(mutex);
        hitches.clear();

    }

    void HitchRecorder::LogHitch(const Hitch& hitch) {

        char line[256];
        std::snprintf(line, sizeof(line), "Hitch: %.1f ms, average %.1f ms, at %.1f s", hitch.frameTime,
            hitch.averageFrameTime, hitch.time);
        std::string text = line;

        // Only the scopes that took a noticeable part of the frame, the slowest first
        auto minimumTime = double(hitch.frameTime) * double(logMinimumShare) * 1000000.0;
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

        Log::Warning(text);

    }

}
