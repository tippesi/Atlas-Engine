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

        // Forgets the edit of the last frame
        void ClearEdit();

        // Sets the axes which were edited in this frame for other entities as well
        void ApplyEdit(const std::vector<Scene::Entity>& entities);

        // Decomposes the matrix again on the next render, e.g. after a tool moved the entity
        void Refresh() { lastEntity = Scene::Entity(); }

    private:
        Scene::Entity lastEntity;

        vec3 position;
        vec3 rotation;
        vec3 scale;

        glm::bvec3 editedPosition = glm::bvec3(false);
        glm::bvec3 editedRotation = glm::bvec3(false);
        glm::bvec3 editedScale = glm::bvec3(false);

    };

}
