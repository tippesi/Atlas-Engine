#include "ProfilerWindow.h"

#include "Clock.h"
#include "graphics/Profiler.h"

namespace Atlas::Editor::UI {

    void ProfilerWindow::Render() {

        if (!Begin())
            return;

        auto frameTime = Clock::GetAverage() * 1000.0f;
        auto fps = frameTime > 0.0f ? 1000.0f / frameTime : 0.0f;

        // GPU time is the time of the slowest queue
        double gpuTime = 0.0;
        auto gpuProfilerData = Graphics::Profiler::GetQueriesAverage(32, Graphics::Profiler::OrderBy::MAX_TIME);
        for (const auto& threadData : gpuProfilerData) {
            double threadTime = 0.0;
            for (const auto& query : threadData.queries)
                threadTime += double(query.timer.elapsedTime);
            gpuTime = std::max(gpuTime, threadTime);
        }

        // Summary tiles: muted label on top, large value below
        if (ImGui::BeginTable("Profiler summary", 3, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
            auto tile = [](const char* label, const char* format, float value) {
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", label);
                ImGui::SetWindowFontScale(1.6f);
                ImGui::Text(format, value);
                ImGui::SetWindowFontScale(1.0f);
                };

            tile("Frame time", "%.2f ms", frameTime);
            tile("Frame rate", "%.0f fps", fps);
            if (Graphics::Profiler::enable)
                tile("GPU time", "%.2f ms", float(gpuTime / 1000000.0));
            else {
                ImGui::TableNextColumn();
                ImGui::TextDisabled("GPU time");
                ImGui::SetWindowFontScale(1.6f);
                ImGui::TextDisabled("-");
                ImGui::SetWindowFontScale(1.0f);
            }

            ImGui::EndTable();
        }

        ImGui::SeparatorText("GPU timings");

        gpuProfilerPanel.Render();

        End();

    }

}
