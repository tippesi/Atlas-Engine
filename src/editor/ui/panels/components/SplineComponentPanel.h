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

        bool CheckRayIntersection();

        void ConfirmRayIntersection();

    private:
        bool RenderControlPoint(SplineControlPoint& point, size_t idx);

    };

}