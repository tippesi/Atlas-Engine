#pragma once

#include "Panel.h"

#include <deque>

namespace Atlas::ImguiExtension {

    class PerformanceGraphPanel : public Panel {

    public:
        PerformanceGraphPanel() : Panel("GPU profiler hierarchy") {}

        void Render(ivec2 size = ivec2(0), float alpha = -1.0f);

        void RenderGraph(ivec2 size, float alpha);

        void UpdateGraphData();

        bool showGraph = false;

        int32_t timeWindowSize = 1024;

        float maxFrameTime = 0.0f;
        float maxGpuTime = 0.0f;

        std::deque<float> frameTimes;
        std::deque<float> gpuTimes;

    };

}