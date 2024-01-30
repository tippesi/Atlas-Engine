#pragma once

#include "Panel.h"
#include "scene/Scene.h"

namespace Atlas::Editor::UI {

    class EntityPropertiesPanel : public Panel {

    public:
        EntityPropertiesPanel() : Panel("Entity properties") {}

        void Render(Scene::Entity entity);

    };

}