#pragma once

#include "../Panel.h"

#include "scene/components/LightComponent.h"

namespace Atlas::Editor::UI {

    class LightComponentPanel : public Panel {

    public:
        LightComponentPanel() : Panel("Light component") {}

        bool Render(LightComponent& lightComponent);

    };

}