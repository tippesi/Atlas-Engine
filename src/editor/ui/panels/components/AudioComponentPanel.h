#pragma once

#include "../Panel.h"

#include "scene/Scene.h"
#include "scene/components/AudioComponent.h"

namespace Atlas::Editor::UI {

    class AudioComponentPanel : public Panel {

    public:
        AudioComponentPanel() : Panel("Audio component") {}

        bool Render(Ref<Scene::Scene>& scene, Scene::Entity entity, AudioComponent& audioVolumeComponent);

    };

}
