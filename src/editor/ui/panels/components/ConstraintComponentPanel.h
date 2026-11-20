#pragma once

#include "../Panel.h"

#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/components/ConstraintComponent.h"

namespace Atlas::Editor::UI {

    class ConstraintComponentPanel : public Panel {

    public:
        ConstraintComponentPanel() : Panel("Constraint component") {}

        bool Render(Ref<Scene::Scene>& scene, Scene::Entity entity, ConstraintComponent& constraintComponent);

    private:
        bool RenderConnectedEntity(Scene::Entity entity, ConstraintComponent& constraintComponent);

        bool RenderConstraintSettings(Physics::ConstraintCreationSettings& settings);

    };

}
