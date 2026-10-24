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
        bool ocean = false;
        bool wind = false;
        bool sky = false;
        bool postProcessing = false;
    };

    enum class EntitySelectionType {
        Entity = 0,
        Spline
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
        EntitySelectionType selectedType = EntitySelectionType::Entity;

        std::unordered_set<ECS::Entity> selectedEntities;
        SelectedProperty selectedProperty;

        HierarchyFilter hierarchyFilter = HierarchyFilterBits::AllBit;

        // Set when the user wants to focus the camera on the selection (double click, context menu)
        bool focusRequested = false;

        // Predefined entities that can be created from the hierarchy, the command palette etc.
        enum class EntityTemplate {
            Empty = 0,
            PointLight,
            SpotLight,
            DirectionalLight,
            Camera,
            AudioSource,
            Text
        };

        // Set by the create menus. The scene window creates the entity, since it knows the editor camera for the placement
        struct CreateRequest {
            EntityTemplate type = EntityTemplate::Empty;
            Scene::Entity parent;
            bool pending = false;
        };
        CreateRequest createRequest;

        // Menu items for all entity templates, returns true if one was chosen
        bool RenderCreateMenuItems(Scene::Entity parent);

    private:
        void StartRename(Scene::Entity entity);

        // Inline renaming of an entity in the hierarchy
        Scene::Entity renamingEntity;
        std::string renameBuffer;
        bool renameFocusPending = false;
        int32_t renameFrameCount = 0;

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
            std::string search;
            HierarchyFilter filter;
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

        JobGroup searchJob{ "Scene hierarchy search", JobPriority::Low };

        std::string entitySearch;
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