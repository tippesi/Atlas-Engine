#pragma once

#include "../Panel.h"

#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/components/PlayerComponent.h"

namespace Atlas::Editor::UI {

    class PlayerComponentPanel : public Panel {

    public:
        PlayerComponentPanel() : Panel("Player component") {}

        bool Render(Ref<Scene::Scene>& scene, Scene::Entity entity, PlayerComponent& playerComponent);

    private:
        void RenderShapeSettings(Scene::Entity entity, Physics::PlayerCreationSettings& creationSettings);

    };

}