#pragma once

#include "Panel.h"
#include "PerformanceGraphPanel.h"

#include <deque>
#include <functional>
#include <algorithm>

namespace Atlas::ImguiExtension {

    class GPUProfilerPanel : public Panel {

    public:
        GPUProfilerPanel() : Panel("GPU profiler hierarchy") {}

        void Render();

        void RenderTable();

        bool showGraph = false;

        int32_t sortItem = 0;

        PerformanceGraphPanel perfGraphPanel;

    };

}