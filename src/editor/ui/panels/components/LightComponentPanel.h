#pragma once

#include "../Panel.h"

#include "scene/Scene.h"
#include "scene/components/LightComponent.h"

namespace Atlas::Editor::UI {

    class LightComponentPanel : public Panel {

    public:
        LightComponentPanel() : Panel("Light component") {}

        bool Render(Ref<Scene::Scene>& scene, Scene::Entity entity, LightComponent& lightComponent);

    };

}