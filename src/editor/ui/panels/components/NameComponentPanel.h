#pragma once

#include "../Panel.h"

#include "scene/Scene.h"
#include "scene/components/NameComponent.h"

namespace Atlas::Editor::UI {

    class NameComponentPanel : public Panel {

    public:
        NameComponentPanel() : Panel("Name component") {}

        bool Render(Ref<Scene::Scene>& scene, Scene::Entity entity, NameComponent& nameComponent);

    };

}