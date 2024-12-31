#pragma once

#include "Panel.h"
#include "ScenePropertiesPanel.h"
#include "scene/Scene.h"

namespace Atlas::Editor::UI {

    struct SelectedProperty {
        bool terrain = false;
        bool fog = false;
        bool volumetricClouds = false;
        bool reflection = false;
        bool ssgi = false;
        bool rtgi = false;
        bool sss = false;
        bool irradianceVolume = false;
        bool wind = false;
        bool sky = false;
        bool postProcessing = false;
    };

    class SceneHierarchyPanel : public Panel {

    public:
        typedef uint32_t HierarchyFilter;

        typedef enum HierarchyFilterBits {
            NameBit = (1 << 0),
            TransformBit = (1 << 1),
            MeshBit = (1 << 2),
            TextBit = (1 << 3),
            AudioBit = (1 << 4),
            AudioVolumeBit = (1 << 5),
            PlayerBit = (1 << 6),
            RigidBodyBit = (1 << 7),
            CameraBit = (1 << 8),
            ScriptBit = (1 << 9),
            LightBit = (1 << 10),
            AllBit = (1 << 11) - 1
        } HierarchyFilterBits;

        SceneHierarchyPanel() : Panel("Scene hierarchy") {}

        ~SceneHierarchyPanel() { JobSystem::Wait(searchJob); }

        void Update(Ref<Scene::Scene>& scene);

        void Render(Ref<Scene::Scene>& scene, bool inFocus);

        void SelectEntity(Scene::Entity entity, bool keepSelection = false);

        void ClearSelection();

        Scene::Entity selectedEntity;
        std::unordered_set<ECS::Entity> selectedEntities;
        SelectedProperty selectedProperty;

        HierarchyFilter hierarchyFilter = HierarchyFilterBits::AllBit;

    private:
        struct EntityData {
            ECS::Pool<HierarchyComponent> hierarchyPool;
            ECS::Pool<NameComponent> namePool;
            ECS::Pool<TransformComponent> transformPool;
            ECS::Pool<MeshComponent> meshPool;
            ECS::Pool<TextComponent> textPool;
            ECS::Pool<AudioComponent> audioPool;
            ECS::Pool<AudioVolumeComponent> audioVolumePool;
            ECS::Pool<PlayerComponent> playerPool;
            ECS::Pool<RigidBodyComponent> rigidBodyPool;
            ECS::Pool<CameraComponent> cameraPool;
            ECS::Pool<LuaScriptComponent> scriptPool;
            ECS::Pool<LightComponent> lightPool;
        };        

        void TraverseHierarchy(Ref<Scene::Scene>& scene, Scene::Entity entity,
            std::unordered_set<ECS::Entity>& matchSet, bool inFocus, bool searchChanged, bool* selectionChanged);

        void RenderExtendedHierarchy(const Ref<Scene::Scene>& scene, bool* selectionChanged);

        void RenderExtendedItem(const std::string& name, bool* selected, bool* selectionChanged);

        void RenderFilterPopup();

        void DeleteSelectedEntities(Ref<Scene::Scene>& scene);

        void DuplicateSelectedEntity(Ref<Scene::Scene>& scene);

        bool SearchHierarchy(EntityData& data, ECS::Entity entity,
            std::unordered_set<ECS::Entity>& matchSet, std::string& nodeName, bool parentMatches);

        void ToggleHierarchyVisibility(Scene::Entity entity, bool visible);

        JobGroup searchJob{ JobPriority::Low };

        std::string entitySearch;
        std::string transformedEntitySearch;
        std::unordered_set<ECS::Entity> matchSet;
        std::unordered_set<ECS::Entity> newMatchSet;
        std::unordered_set<ECS::Entity> nodeInvisibleSet;
        std::unordered_set<ECS::Entity> toggledOpenSet;

        int32_t lastAliveEntityCount = 0;
        std::string lastEntitySearch;
        bool hierarchyFilterChanged = false;
        bool searchJobResultRetrieved = true;

    };

}