#pragma once

#include "Panel.h"
#include "PerformanceGraphPanel.h"

#include "profiler/Profiler.h"

#include <atomic>

namespace Atlas::ImguiExtension {

    class ProfilerPanel : public Panel {

    public:
        ProfilerPanel() : Panel("Profiler") {}

        void Render();

        bool showGraph = false;

        int32_t sortItem = 0;

        PerformanceGraphPanel perfGraphPanel;

    private:
        bool RenderControls(std::atomic_bool& enable, bool graph);

        void RenderTable(const std::vector<Profiler::ThreadData>& threadData, bool openAllThreads);

        Profiler::OrderBy GetOrder() const;

    };

}
