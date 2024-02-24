#pragma once

#include "Window.h"

namespace Atlas::Editor::UI {

    class GeometryBrushWindow : public Window {

    public:
        explicit GeometryBrushWindow(bool show) : Window("Geometry brush", show) {}

        void Render();

    private:
        

    };

}