#pragma once

#include "../Panel.h"

#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/components/SplineComponent.h"

namespace Atlas::Editor::UI {

    class SplineComponentPanel : public Panel {

    public:
        SplineComponentPanel() : Panel("Spline component") {}

        bool Render(Ref<Scene::Scene>& scene, Scene::Entity entity, SplineComponent& splineComponent);

        int32_t selectControlPointIdx = -1;

    private:
        enum class ControlPointAction {
            None = 0,
            Delete,
            MoveUp,
            MoveDown
        };

        void RenderControlPoint(SplineControlPoint& point, size_t idx, ControlPointAction& action);

    };

}