#pragma once

#include "../Panel.h"

#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/components/MeshComponent.h"

namespace Atlas::Editor::UI {

    class MeshComponentPanel : public Panel {

    public:
        MeshComponentPanel() : Panel("Mesh component") {}

        bool Render(Ref<Scene::Scene>& scene, Scene::Entity entity, MeshComponent& meshComponent);

    };

}