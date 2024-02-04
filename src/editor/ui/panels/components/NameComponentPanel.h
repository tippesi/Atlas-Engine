#pragma once

#include "../Panel.h"

#include "scene/components/NameComponent.h"

namespace Atlas::Editor::UI {

    class NameComponentPanel : public Panel {

    public:
        NameComponentPanel() : Panel("Name component") {}

        bool Render(Scene::Entity entity, NameComponent& nameComponent);

    };

}