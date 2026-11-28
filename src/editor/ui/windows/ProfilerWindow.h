#pragma once

#include "DrawerWindow.h"

#include <ImguiExtension/panels/ProfilerPanel.h>

namespace Atlas::Editor::UI {

    class ProfilerWindow : public DrawerWindow {

    public:
        explicit ProfilerWindow(bool show) : DrawerWindow("Profiler", show) {}

        void Render();

    private:
        ImguiExtension::ProfilerPanel profilerPanel;

    };

}