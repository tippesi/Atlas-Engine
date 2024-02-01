#pragma once

#include "Panel.h"
#include "scene/Scene.h"

#include "EntityPropertiesPanel.h"

namespace Atlas::Editor::UI {

    class ScenePropertiesPanel : public Panel {

    public:
        ScenePropertiesPanel() : Panel("Scene properties") {}

        void Render(Scene::Entity entity);

    private:
        EntityPropertiesPanel entityPropertiesPanel;

    };

}