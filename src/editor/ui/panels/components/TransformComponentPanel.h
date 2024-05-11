#pragma once

#include "../Panel.h"

#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/components/TransformComponent.h"

namespace Atlas::Editor::UI {

    class TransformComponentPanel : public Panel {

    public:
        TransformComponentPanel() : Panel("Transform component") {}

        bool Render(Ref<Scene::Scene>& scene, Scene::Entity entity, TransformComponent& transform);

    private:
        Scene::Entity lastEntity;

        vec3 position;
        vec3 rotation;
        vec3 scale;
        quat quaternion;

        glm::mat4 lastTransform;

    };

}
