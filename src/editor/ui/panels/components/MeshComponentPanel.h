#pragma once

#include "../Panel.h"

#include "scene/Entity.h"
#include "scene/components/MeshComponent.h"

namespace Atlas::Editor::UI {

    class MeshComponentPanel : public Panel {

    public:
        MeshComponentPanel() : Panel("Mesh component") {}

        bool Render(Scene::Entity entity, MeshComponent& meshComponent);

    };

}