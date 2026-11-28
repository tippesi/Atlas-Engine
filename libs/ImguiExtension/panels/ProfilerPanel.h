#pragma once

#include "Panel.h"
#include "PerformanceGraphPanel.h"

#include "profiler/Profiler.h"
#include "profiler/HitchRecorder.h"

#include <atomic>
#include <limits>

namespace Atlas::ImguiExtension {

    class ProfilerPanel : public Panel {

    public:
        ProfilerPanel() : Panel("Profiler") {}

        void Render();

        // Averages of the queries over the history, refreshed a few times per second: merging the history of all
        // threads every frame costs more than most of the work it measures
        void Refresh();

        std::vector<Profiler::ThreadData> cpuThreads;
        std::vector<Profiler::ThreadData> gpuThreads;

        bool showGraph = false;

        int32_t sortItem = 0;

        PerformanceGraphPanel perfGraphPanel;

    private:
        bool RenderControls(std::atomic_bool& enable, bool graph);

        void RenderHitches();

        // Returns whether the settings changed
        bool RenderHitchSettings(HitchRecorder::Settings& settings, bool sideBySide);

        // A height of 0 fits the table to its rows, at most a few
        void RenderHitchSummary(float height);

        void RenderHitchList(float height, bool recording);

        void RenderHitchDetails();

        // The selected hitch or the latest one
        const HitchRecorder::Hitch* GetSelectedHitch() const;

        void RenderTable(const std::vector<Profiler::ThreadData>& threadData, bool openAllThreads);

        Profiler::OrderBy GetOrder() const;

        // The recorded hitches, updated when the recorder changes
        std::vector<Ref<const HitchRecorder::Hitch>> hitches;
        std::vector<HitchRecorder::CauseSummary> hitchSummary;
        uint64_t hitchRevision = std::numeric_limits<uint64_t>::max();
        uint64_t selectedHitchId = std::numeric_limits<uint64_t>::max();

        float lastRefreshTime = -1.0f;
        Profiler::OrderBy refreshedOrder = Profiler::OrderBy::MAX_TIME;

    };

}
