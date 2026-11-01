#include "ProfilerWindow.h"

#include "Clock.h"
#include "profiler/CPUProfiler.h"
#include "profiler/GPUProfiler.h"

namespace Atlas::Editor::UI {

    void ProfilerWindow::Render() {

        if (!Begin())
            return;

        auto frameTime = Clock::GetAverage() * 1000.0f;
        auto fps = frameTime > 0.0f ? 1000.0f / frameTime : 0.0f;

        // CPU time is the time of the main thread, GPU time the time of the slowest queue
        double cpuTime = 0.0;
        for (const auto& threadData : CPUProfiler::GetQueriesAverage(32)) {
            if (threadData.name != "Main")
                continue;
            for (const auto& query : threadData.queries)
                cpuTime += double(query.elapsedTime);
        }

        double gpuTime = 0.0;
        auto gpuProfilerData = GPUProfiler::GetQueriesAverage(32, GPUProfiler::OrderBy::MAX_TIME);
        for (const auto& threadData : gpuProfilerData) {
            double threadTime = 0.0;
            for (const auto& query : threadData.queries)
                threadTime += double(query.elapsedTime);
            gpuTime = std::max(gpuTime, threadTime);
        }

        // Summary tiles: muted label on top, large value below
        if (ImGui::BeginTable("Profiler summary", 4, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
            auto tile = [](const char* label, const char* format, float value) {
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", label);
                ImGui::SetWindowFontScale(1.6f);
                ImGui::Text(format, value);
                ImGui::SetWindowFontScale(1.0f);
                };

            auto disabledTile = [](const char* label) {
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", label);
                ImGui::SetWindowFontScale(1.6f);
                ImGui::TextDisabled("-");
                ImGui::SetWindowFontScale(1.0f);
                };

            tile("Frame time", "%.2f ms", frameTime);
            tile("Frame rate", "%.0f fps", fps);
            if (CPUProfiler::enable)
                tile("CPU main thread", "%.2f ms", float(cpuTime / 1000000.0));
            else
                disabledTile("CPU main thread");
            if (GPUProfiler::enable)
                tile("GPU time", "%.2f ms", float(gpuTime / 1000000.0));
            else
                disabledTile("GPU time");

            ImGui::EndTable();
        }

        profilerPanel.Render();

        End();

    }

}
