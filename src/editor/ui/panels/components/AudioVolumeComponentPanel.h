#pragma once

#include "../Panel.h"

#include "scene/components/AudioVolumeComponent.h"

namespace Atlas::Editor::UI {

    class AudioVolumeComponentPanel : public Panel {

    public:
        AudioVolumeComponentPanel() : Panel("Audio volume component") {}

        bool Render(Scene::Entity entity, AudioVolumeComponent& audioVolumeComponent);

    };

}
