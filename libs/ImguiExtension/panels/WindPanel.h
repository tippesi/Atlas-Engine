#pragma once

#include "Panel.h"

#include "scene/Wind.h"

namespace Atlas::ImguiExtension {

    class WindPanel : public Panel {

    public:
        WindPanel() : Panel("Wind properties") {}

        void Render(Ref<ImguiWrapper>& wrapper, Scene::Wind& wind);

    };

}