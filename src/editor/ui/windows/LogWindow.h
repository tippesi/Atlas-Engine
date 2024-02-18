#pragma once

#include "Window.h"

namespace Atlas::Editor::UI {

    class LogWindow : public Window {

    public:
        explicit LogWindow(bool show) : Window("Log", show) {}

        void Render();

    private:
        std::string logSearch;

    };

}