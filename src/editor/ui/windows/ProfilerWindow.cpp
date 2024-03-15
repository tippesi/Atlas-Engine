#include "ProfilerWindow.h"

#include "Clock.h"

namespace Atlas::Editor::UI {

    void ProfilerWindow::Render() {

        if (!Begin())
            return;

        ImGui::Text("Total frame time: %f ms", Clock::GetAverage() * 1000.0f);

        ImGui::Separator();

        ImGui::Text("GPU time table");

        gpuProfilerPanel.Render();

        End();

    }

}