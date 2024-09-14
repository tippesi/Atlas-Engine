#include "GPUProfilerPanel.h"

#include "graphics/Profiler.h"
#include "Clock.h"

namespace Atlas::ImguiExtension {

    void GPUProfilerPanel::Render() {

        ImGui::PushID(GetNameID());

        bool enabled = Graphics::Profiler::enable;
        ImGui::Checkbox("Enable##Profiler", &enabled);
        ImGui::Checkbox("Show graph##Profiler", &showGraph);
        Graphics::Profiler::enable = enabled;

        UpdateGraphData();

        if (showGraph)
            RenderGraph();
        else
            RenderTable();

        ImGui::PopID();

    }

    void GPUProfilerPanel::RenderTable() {

        const char* items[] = { "Chronologically", "Max time", "Min time" };
        static int item = 0;
        ImGui::Combo("Sort##Performance", &item, items, IM_ARRAYSIZE(items));

        Graphics::Profiler::OrderBy order;
        switch (item) {
            case 1: order = Graphics::Profiler::OrderBy::MAX_TIME; break;
            case 2: order = Graphics::Profiler::OrderBy::MIN_TIME; break;
            default: order = Graphics::Profiler::OrderBy::CHRONO; break;
        }

        std::function<void(Graphics::Profiler::Query&)> displayQuery;
        displayQuery = [&displayQuery](Graphics::Profiler::Query& query) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGuiTreeNodeFlags expandable = 0;
            if (!query.children.size()) expandable = ImGuiTreeNodeFlags_NoTreePushOnOpen |
                ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

            bool open = ImGui::TreeNodeEx(query.name.c_str(), expandable | ImGuiTreeNodeFlags_SpanFullWidth);
            ImGui::TableNextColumn();
            ImGui::Text("%f", double(query.timer.elapsedTime) / 1000000.0);

            if (open && query.children.size()) {
                for (auto& child : query.children)
                    displayQuery(child);
                ImGui::TreePop();
            }

        };

        static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH |
            ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

        if (ImGui::BeginTable("PerfTable", 2, flags)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
            ImGui::TableSetupColumn("Elapsed (ms)", ImGuiTableColumnFlags_NoHide);
            ImGui::TableHeadersRow();

            auto threadData = Graphics::Profiler::GetQueriesAverage(64, order);
            for (auto& thread : threadData) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGuiTreeNodeFlags expandable = 0;
                if (!thread.queries.size()) expandable = ImGuiTreeNodeFlags_NoTreePushOnOpen |
                    ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

                bool open = ImGui::TreeNodeEx(thread.name.c_str(), expandable | ImGuiTreeNodeFlags_SpanFullWidth);

                if (open && thread.queries.size()) {
                    for (auto &query: thread.queries)
                        displayQuery(query);
                    ImGui::TreePop();
                }
            }


            ImGui::EndTable();
        }

    }

    void GPUProfilerPanel::RenderGraph() {

        auto dataSize = int32_t(frameTimes.size());

        std::vector<float> localFrameTimes(frameTimes.begin(), frameTimes.end());
        std::vector<float> localGpuTimes(gpuTimes.begin(), gpuTimes.end());

        auto availSize = ImGui::GetContentRegionAvail();

        auto frameColor = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        ImPlot::PushStyleColor(ImPlotCol_PlotBorder, frameColor);
        ImPlot::PushStyleColor(ImPlotCol_FrameBg, frameColor);
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

    void GPUProfilerPanel::UpdateGraphData() {

        auto gpuProfilerData = Graphics::Profiler::GetQueries(Graphics::Profiler::OrderBy::MAX_TIME);

        int32_t slowestThreadIdx = 0;
        double slowestTime = 0.0;
        for (int32_t i = 0; i < int32_t(gpuProfilerData.size()); i++) {
            const auto& threadData = gpuProfilerData[i];
            double threadTime = 0.0;
            for (const auto& query : threadData.queries) {
                threadTime += query.timer.elapsedTime;
            }
            if (threadTime > slowestTime) {
                slowestTime = threadTime;
                slowestThreadIdx = i;
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
        for (int32_t i = 0; i < timeWindowSize; i++) {
            maxFrameTime = std::max(frameTimes[i], maxFrameTime);
            maxGpuTime = std::max(gpuTimes[i], maxGpuTime);
        }

    }

}