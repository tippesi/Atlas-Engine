#pragma once

#include "Panel.h"
#include "scene/Scene.h"

namespace Atlas::Editor::UI {

    class SceneHierarchyPanel : public Panel {

    public:
        SceneHierarchyPanel() : Panel("Entity properties") {}

        void Render(Ref<Scene::Scene>& scene);

    };

}