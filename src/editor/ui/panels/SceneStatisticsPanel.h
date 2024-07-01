#pragma once

#include "Panel.h"
#include "scene/Scene.h"

namespace Atlas::Editor::UI {

    class SceneStatisticsPanel : public Panel {

    public:
        SceneStatisticsPanel() : Panel("Scene statistics") {}

        void Render(Ref<Scene::Scene> scene);

    private:

    };

}