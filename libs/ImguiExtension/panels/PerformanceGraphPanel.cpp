#include "PerformanceGraphPanel.h"

#include "graphics/Profiler.h"
#include "Clock.h"

namespace Atlas::ImguiExtension {

    void PerformanceGraphPanel::Render(ivec2 size, float alpha) {

        ImGui::PushID(GetNameID());

        UpdateGraphData();

        RenderGraph(size, alpha);

        ImGui::PopID();

    }

    void PerformanceGraphPanel::RenderGraph(ivec2 size, float alpha) {

        auto dataSize = int32_t(frameTimes.size());

        std::vector<float> localFrameTimes(frameTimes.begin(), frameTimes.end());
        std::vector<float> localGpuTimes(gpuTimes.begin(), gpuTimes.end());

        auto availSize = ImGui::GetContentRegionAvail();
        if (size != ivec2(0))
            availSize = ImVec2(float(size.x), float(size.y));

        auto frameColor = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        if (alpha >= 0.0f)
            frameColor.w = alpha;

        ImPlot::PushStyleColor(ImPlotCol_PlotBorder, frameColor);
        ImPlot::PushStyleColor(ImPlotCol_FrameBg, frameColor);
        ImPlot::PushStyleColor(ImPlotCol_PlotBg, frameColor);
        ImPlot::PushStyleColor(ImPlotCol_Fill, frameColor);
        ImPlot::PushStyleVar(ImPlotStyleVar_PlotBorderSize, 0.0f);

        if (ImPlot::BeginPlot("Performance graph", availSize)) {
            ImPlot::SetupAxes("Frames", "Time (ms)");
            ImPlot::SetupAxesLimits(0, timeWindowSize, 0, std::min(200.0f, maxFrameTime), ImPlotCond_Always);
            ImPlot::SetupLegend(ImPlotLocation_SouthWest);

            ImPlot::PlotLine("Frame time", localFrameTimes.data(), dataSize);
            ImPlot::PlotLine("GPU time", localGpuTimes.data(), dataSize);

            ImPlot::EndPlot();
        }

        ImPlot::PopStyleVar();
        ImPlot::PopStyleColor();
        ImPlot::PopStyleColor();

    }

    void PerformanceGraphPanel::UpdateGraphData() {

        auto gpuProfilerData = Graphics::Profiler::GetQueries(Graphics::Profiler::OrderBy::MAX_TIME);

        double slowestTime = 0.0;
        for (int32_t i = 0; i < int32_t(gpuProfilerData.size()); i++) {
            const auto& threadData = gpuProfilerData[i];
            double threadTime = 0.0;
            for (const auto& query : threadData.queries) {
                threadTime += query.timer.elapsedTime;
            }
            if (threadTime > slowestTime) {
                slowestTime = threadTime;
            }
        }

        frameTimes.push_back(Clock::GetDelta() * 1000.0f);
        gpuTimes.push_back(float(slowestTime / 1000000.0));

        if (int32_t(frameTimes.size()) > timeWindowSize) {
            frameTimes.pop_front();
            gpuTimes.pop_front();
        }

        maxFrameTime = 0.0f;
        maxGpuTime = 0.0f;
        for (int32_t i = 0; i < timeWindowSize && i < int32_t(frameTimes.size()); i++) {
            maxFrameTime = std::max(frameTimes[i], maxFrameTime);
            maxGpuTime = std::max(gpuTimes[i], maxGpuTime);
        }

    }

}