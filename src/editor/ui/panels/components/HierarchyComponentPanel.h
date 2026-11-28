#pragma once

#include "../Panel.h"

#include "scene/Scene.h"
#include "scene/components/HierarchyComponent.h"

namespace Atlas::Editor::UI {

    class HierarchyComponentPanel : public Panel {

    public:
        HierarchyComponentPanel() : Panel("Hierarchy component") {}

        bool Render(const Ref<Scene::Scene>& scene, Scene::Entity entity, HierarchyComponent& component);

    };

}
