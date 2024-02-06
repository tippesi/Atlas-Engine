#pragma once

#include "Panel.h"
#include "scene/Scene.h"

namespace Atlas::Editor::UI {

    class SceneHierarchyPanel : public Panel {

    public:
        SceneHierarchyPanel() : Panel("Scene hierarchy") {}

        void Render(Ref<Scene::Scene>& scene);

        Scene::Entity selectedEntity;

    private:
        void TraverseHierarchy(Ref<Scene::Scene>& scene, Scene::Entity entity);

    };

}