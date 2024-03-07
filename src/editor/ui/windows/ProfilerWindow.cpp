#include "ProfilerWindow.h"

#include "Clock.h"

namespace Atlas::Editor::UI {

    void ProfilerWindow::Render() {

        if (!Begin())
            return;

        ImGui::Text("Frametime: %f ms", Clock::GetAverage() * 1000.0f);

        gpuProfilerPanel.Render();

        End();

    }

}