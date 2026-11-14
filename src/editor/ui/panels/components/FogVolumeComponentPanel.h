#pragma once

#include "../Panel.h"

#include "scene/Scene.h"
#include "scene/components/FogVolumeComponent.h"

namespace Atlas::Editor::UI {

    class FogVolumeComponentPanel : public Panel {

    public:
        FogVolumeComponentPanel() : Panel("Fog volume component") {}

        bool Render(Ref<Scene::Scene>& scene, Scene::Entity entity, FogVolumeComponent& fogVolumeComponent);

    };

}
