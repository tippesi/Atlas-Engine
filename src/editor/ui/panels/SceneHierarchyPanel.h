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

        void Render(Ref<Scene::Scene>& scene, bool inFocus);

        Scene::Entity selectedEntity;
        SelectedProperty selectedProperty;

    private:
        void TraverseHierarchy(Ref<Scene::Scene>& scene, Scene::Entity entity, bool inFocus);

        void RenderExtendedHierarchy(const Ref<Scene::Scene>& scene);

        void RenderExtendedItem(const std::string& name, bool* selected);

        std::string entitySearch;

    };

}