#pragma once

#include "Panel.h"
#include "ScenePropertiesPanel.h"
#include "../../SceneHistory.h"
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
        bool navigation = false;
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
        SceneHierarchyPanel() : Panel("Scene hierarchy") {}

        ~SceneHierarchyPanel() { JobSystem::Wait(searchJob); }

        void Update(Ref<Scene::Scene>& scene);

        void Render(Ref<Scene::Scene>& scene);

        void SelectEntity(Scene::Entity entity, bool keepSelection = false);

        void ClearSelection();

        void StartRename(Scene::Entity entity);

        void DeleteSelectedEntities(const Ref<Scene::Scene>& scene);

        void DuplicateSelectedEntity(const Ref<Scene::Scene>& scene);

        // Opens the parents of the entity and scrolls to it in the next frame
        void RevealEntity(const Ref<Scene::Scene>& scene, Scene::Entity entity);

        Scene::Entity selectedEntity;
        EntitySelectionType selectedType = EntitySelectionType::Entity;

        std::unordered_set<ECS::Entity> selectedEntities;
        SelectedProperty selectedProperty;

        // Prefabs have no scene settings of their own
        bool showSceneSettings = true;

        // Bit i shows the entities with the component of Scene::GetComponentTypes()[i]
        uint32_t hierarchyFilter = ~0u;

        // Set when the hierarchy changed the scene, e.g. by deleting or renaming entities
        bool sceneChanged = false;

        // Records the changes of the hierarchy, set by the scene window
        SceneHistory* history = nullptr;

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
            Text,
            FogVolume
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
        ECS::Entity revealEntity = ECS::EntityConfig::InvalidEntity;
        std::unordered_set<ECS::Entity> revealOpenSet;

        // Inline renaming of an entity in the hierarchy
        Scene::Entity renamingEntity;
        std::string renameBuffer;
        bool renameFocusPending = false;
        int32_t renameFrameCount = 0;

        // Copies for the search job. The filters only need to know which entities have a component, copies of
        // components like scripts hold references into other systems, which would be released on the job thread
        struct EntityData {
            ECS::Pool<HierarchyComponent> hierarchyPool;
            ECS::Pool<NameComponent> namePool;
            std::vector<ECS::Storage> componentStorages;
            std::string search;
            uint32_t filter;
        };

        static uint32_t GetAllFilterBits();

        // Goes through the rows of the open tree, only the ones in the window are rendered
        void TraverseHierarchy(Ref<Scene::Scene>& scene, Scene::Entity entity, bool* selectionChanged);

        void TraverseChildren(Ref<Scene::Scene>& scene, Scene::Entity entity, bool* selectionChanged);

        void RenderEntityRow(Ref<Scene::Scene>& scene, Scene::Entity entity, bool* selectionChanged);

        // Advances the layout by the rows that weren't rendered
        void FlushSkippedRows();

        // Open entities whose rows were scrolled out stay visible at the top of the tree
        void RenderStickyRows(Ref<Scene::Scene>& scene, bool* selectionChanged);

        // The entity dropped on the last item, invalid without a drop
        Scene::Entity AcceptEntityDrop(const Ref<Scene::Scene>& scene);

        // Moves the dropped entity, or the selection if it's part of it, below the parent
        void MoveEntities(Ref<Scene::Scene>& scene, Scene::Entity parent, Scene::Entity droppedEntity);

        void RenderExtendedHierarchy(const Ref<Scene::Scene>& scene, bool* selectionChanged);

        void RenderExtendedItem(const std::string& name, IconType iconType, bool* selected, bool* selectionChanged);

        void RenderFilterPopup();

        bool SearchHierarchy(EntityData& data, ECS::Entity entity,
            std::unordered_set<ECS::Entity>& matchSet, std::string& nodeName, bool parentMatches);

        void ToggleHierarchyVisibility(Scene::Entity entity, bool visible);

        void CollectMeshEntities(Scene::Entity entity, std::vector<Scene::Entity>& entities);

        JobGroup searchJob{ "Scene hierarchy search", JobPriority::Low };

        std::string entitySearch;
        std::unordered_set<ECS::Entity> matchSet;
        std::unordered_set<ECS::Entity> newMatchSet;
        std::unordered_set<ECS::Entity> nodeInvisibleSet;
        std::unordered_set<ECS::Entity> toggledOpenSet;

        // Without a search or filter there is no match set
        bool matchAll = true;
        // Opens the matches of a new search and closes everything when it's cleared
        bool openSearchMatches = false;
        bool openSearchMatchesPending = false;
        float lastSearchTime = 0.0f;

        ECS::Pool<HierarchyComponent>* hierarchyPool = nullptr;
        ECS::Entity rootEntity = ECS::EntityConfig::InvalidEntity;

        float rowHeight = 20.0f;
        int32_t skippedRows = 0;
        int32_t renderedRows = 0;

        struct OpenRow {
            Scene::Entity entity;
            ImGuiID id;
            ImVec2 position;
        };
        // The open ancestors of the current row and the ones shown at the top of the tree
        std::vector<OpenRow> openRows;
        std::vector<OpenRow> stickyRows;
        bool stickyRowsFound = false;

        int32_t lastAliveEntityCount = 0;
        std::string lastEntitySearch;
        bool hierarchyFilterChanged = false;
        bool searchJobResultRetrieved = true;

    };

}