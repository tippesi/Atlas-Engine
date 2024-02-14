#include "ProfilerWindow.h"

namespace Atlas::Editor::UI {

    void ProfilerWindow::Render() {

        if (!Begin())
            return;

        gpuProfilerPanel.Render();

        End();

    }

}