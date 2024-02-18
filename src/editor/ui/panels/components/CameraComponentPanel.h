#pragma once

#include "../Panel.h"

#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/components/RigidBodyComponent.h"

namespace Atlas::Editor::UI {

    class CameraComponentPanel : public Panel {

    public:
        CameraComponentPanel() : Panel("Camera component") {}

        bool Render(Ref<Scene::Scene>& scene, Scene::Entity entity, CameraComponent& cameraComponent);

    };

}