#pragma once

#include "Panel.h"
#include "scene/Scene.h"

#include "components/NameComponentPanel.h"
#include "components/TransformComponentPanel.h"
#include "components/MeshComponentPanel.h"
#include "components/LightComponentPanel.h"

namespace Atlas::Editor::UI {

    class EntityPropertiesPanel : public Panel {

    public:
        EntityPropertiesPanel() : Panel("Entity properties") {}

        void Render(Scene::Entity entity);

    private:
        NameComponentPanel nameComponentPanel;
        TransformComponentPanel transformComponentPanel;
        MeshComponentPanel meshComponentPanel;
        LightComponentPanel lightComponentPanel;

    };

}