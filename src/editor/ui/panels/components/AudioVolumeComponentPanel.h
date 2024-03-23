#pragma once

#include "../Panel.h"

#include "scene/Scene.h"
#include "scene/components/AudioVolumeComponent.h"

#include "../../popups/ResourceSelectionPopup.h"

namespace Atlas::Editor::UI {

    class AudioVolumeComponentPanel : public Panel {

    public:
        AudioVolumeComponentPanel() : Panel("Audio volume component") {}

        bool Render(Ref<Scene::Scene>& scene, Scene::Entity entity, AudioVolumeComponent& audioVolumeComponent);

    private:
        ResourceSelectionPopup audioSelectionPopup;

    };

}
