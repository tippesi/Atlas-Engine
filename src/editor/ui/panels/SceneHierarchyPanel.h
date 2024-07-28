#pragma once

#include "Panel.h"
#include "ScenePropertiesPanel.h"
#include "scene/Scene.h"

namespace Atlas::Editor::UI {

    struct SelectedProperty {
        bool fog = false;
        bool volumetricClouds = false;
        bool reflection = false;
        bool ssgi = false;
        bool rtgi = false;
        bool sss = false;
        bool irradianceVolume = false;
        bool wind = false;
        bool postProcessing = false;
    };

    class SceneHierarchyPanel : public Panel {

    public:
        SceneHierarchyPanel() : Panel("Scene hierarchy") {}

        void Render(Ref<Scene::Scene>& scene, bool inFocus);

        Scene::Entity selectedEntity;
        SelectedProperty selectedProperty;

    private:
        void TraverseHierarchy(Ref<Scene::Scene>& scene, Scene::Entity entity,
            std::unordered_set<ECS::Entity>& matchSet, bool inFocus, bool* selectionChanged);

        void RenderExtendedHierarchy(const Ref<Scene::Scene>& scene, bool* selectionChanged);

        void RenderExtendedItem(const std::string& name, bool* selected, bool* selectionChanged);

        void DeleteSelectedEntity(Ref<Scene::Scene>& scene);

        void DuplicateSelectedEntity(Ref<Scene::Scene>& scene);

        bool SearchHierarchy(Ref<Scene::Scene>& scene, Scene::Entity entity, 
            std::unordered_set<ECS::Entity>& matchSet, std::string& nodeName, bool parentMatches);

        std::string entitySearch;
        std::string transformedEntitySearch;

    };

}