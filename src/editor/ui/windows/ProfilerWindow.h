#pragma once

#include "Window.h"

#include <ImguiExtension/panels/ProfilerPanel.h>

namespace Atlas::Editor::UI {

    class ProfilerWindow : public Window {

    public:
        explicit ProfilerWindow(bool show) : Window("Profiler", show) {}

        void Render();

    private:
        ImguiExtension::ProfilerPanel profilerPanel;

    };

}