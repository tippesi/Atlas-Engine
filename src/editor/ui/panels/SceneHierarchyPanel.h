#pragma once

#include "Panel.h"
#include "ScenePropertiesPanel.h"
#include "scene/Scene.h"

namespace Atlas::Editor::UI {

    struct SelectedProperty {
        bool fog = false;
        bool volumetricClouds = false;
        bool reflection = false;
        bool irradianceVolume = false;
        bool postProcessing = false;
    };

    class SceneHierarchyPanel : public Panel {

    public:
        SceneHierarchyPanel() : Panel("Scene hierarchy") {}

        void Render(Ref<Scene::Scene>& scene);

        Scene::Entity selectedEntity;
        SelectedProperty selectedProperty;

    private:
        void TraverseHierarchy(Ref<Scene::Scene>& scene, Scene::Entity entity);

        void RenderExtendedHierarchy(Ref<Scene::Scene>& scene);

        void RenderExtendedItem(const std::string& name, bool* selected);

    };

}