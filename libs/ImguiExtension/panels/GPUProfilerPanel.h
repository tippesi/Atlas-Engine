#pragma once

#include "Panel.h"
#include "PerformanceGraphPanel.h"

#include <deque>

namespace Atlas::ImguiExtension {

    class GPUProfilerPanel : public Panel {

    public:
        GPUProfilerPanel() : Panel("GPU profiler hierarchy") {}

        void Render();

        void RenderTable();

        bool showGraph = false;

        PerformanceGraphPanel perfGraphPanel;

    };

}