#pragma once

#include "../Panel.h"

#include "scene/Scene.h"
#include "scene/components/AnimationComponent.h"

#include "../ResourceSelectionPanel.h"

namespace Atlas::Editor::UI {

    class AnimationComponentPanel : public Panel {

    public:
        AnimationComponentPanel() : Panel("Animation component") {}

        bool Render(Ref<Scene::Scene>& scene, Scene::Entity entity, AnimationComponent& animationComponent);

    private:
        ResourceSelectionPanel<Animation::Animation> animationSelectionPanel;

        float blendTime = 0.2f;

    };

}
