#pragma once

#include "Panel.h"

#include "scene/Wind.h"

namespace Atlas::ImguiExtension {

    class WindPanel : public Panel {

    public:
        WindPanel() : Panel("Wind properties") {}

        void Render(Scene::Wind& wind);

    };

}