#pragma once

#include "../Panel.h"

#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/components/TextComponent.h"

namespace Atlas::Editor::UI {

    class TextComponentPanel : public Panel {

    public:
        TextComponentPanel() : Panel("Text component") {}

        bool Render(Ref<Scene::Scene>& scene, Scene::Entity entity, TextComponent& textComponent);

    };

}