#pragma once

#include "Panel.h"

#include "lighting/IrradianceVolume.h"

#include <deque>

namespace Atlas::ImguiExtension {

    class GPUProfilerPanel : public Panel {

    public:
        GPUProfilerPanel() : Panel("GPU profiler hierarchy") {}

        void Render();

    private:
        void RenderTable();

        void RenderGraph();

        void UpdateGraphData();

        bool showGraph = false;

        int32_t timeWindowSize = 1024;

        float maxFrameTime = 0.0f;
        float maxGpuTime = 0.0f;

        std::deque<float> frameTimes;
        std::deque<float> gpuTimes;

    };

}