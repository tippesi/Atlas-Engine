#pragma once

#include "../Panel.h"

#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/components/RigidBodyComponent.h"

namespace Atlas::Editor::UI {

    class RigidBodyComponentPanel : public Panel {

    public:
        RigidBodyComponentPanel() : Panel("Rigid body component") {}

        bool Render(const Ref<Scene::Scene>& scene, Scene::Entity entity, RigidBodyComponent& rigidBodyComponent);

    private:
        void RenderShapeSettings(Scene::Entity entity, Physics::BodyCreationSettings& creationSettings);

        void RenderBodySettings(Scene::Entity entity, Physics::BodyCreationSettings& creationSettings);

    };

}