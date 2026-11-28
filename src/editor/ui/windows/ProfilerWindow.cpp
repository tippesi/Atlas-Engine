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
        profilerPanel.Refresh();
        double cpuTime = 0.0;
        for (const auto& threadData : profilerPanel.cpuThreads) {
            if (threadData.name != "Main")
                continue;
            for (const auto& query : threadData.queries)
                cpuTime += double(query.elapsedTime);
        }

        double gpuTime = 0.0;
        for (const auto& threadData : profilerPanel.gpuThreads) {
            double threadTime = 0.0;
            for (const auto& query : threadData.queries)
                threadTime += double(query.elapsedTime);
            gpuTime = std::max(gpuTime, threadTime);
        }

        // Summary tiles: muted label on top, large value below
        // A drawer has the pin button in its header, the docked window next to the summary
        auto pinWidth = pinned ? GetPinButtonWidth() + ImGui::GetStyle().ItemSpacing.x : 0.0f;
        if (ImGui::BeginTable("Profiler summary", 4, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV,
            ImVec2(-pinWidth, 0.0f))) {
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
        if (pinned) {
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - GetPinButtonWidth());
            RenderPinButton();
        }

        profilerPanel.Render();

        End();

    }

}
