#pragma once

#include "Window.h"

namespace Atlas::Editor::UI {

    class LogWindow : public Window {

    public:
        LogWindow() : Window("Log") {}

        void Render();

    };

}