#pragma once

#include "UndoHistory.h"

#include "scene/Scene.h"
#include "common/SerializationHelper.h"

#include <unordered_map>
#include <unordered_set>

namespace Atlas::Editor {

    // Undo steps of a scene. Restored entities keep their ids, unless another entity took the id in the meantime
    class SceneHistory {

    public:
        // Compares the selected entities and the scene settings with their last state once an interaction finished
        void Update(const Ref<Scene::Scene>& scene, const std::unordered_set<ECS::Entity>& selectedEntities);

        // Structural changes save the hierarchies below the roots before and after the change
        void BeginChange(const Ref<Scene::Scene>& scene, const std::vector<Scene::Entity>& roots);

        void EndChange(const std::string& name, const std::vector<Scene::Entity>& roots);

        // Moves only save the parents and the order of the entities, not their hierarchies
        void BeginMove(const Ref<Scene::Scene>& scene, const std::vector<Scene::Entity>& entities);

        void EndMove(const std::string& name);

        // Component edits of entities which aren't selected, e.g. renaming in the hierarchy
        void BeginEdit(const Ref<Scene::Scene>& scene, const std::vector<Scene::Entity>& entities);

        void EndEdit(const std::string& name);

        bool Undo(const Ref<Scene::Scene>& scene);

        bool Redo(const Ref<Scene::Scene>& scene);

        void Clear();

        // The entities the last undo or redo changed
        std::vector<Scene::Entity> GetChangedEntities();

        UndoHistory history;

    private:
        struct Hierarchy {
            ECS::Entity parent;
            size_t index;
            json entity;
        };

        struct Placement {
            ECS::Entity entity;
            ECS::Entity parent;
            size_t index;
        };

        struct EntityChange {
            ECS::Entity entity;
            json before;
            json after;
        };

        void PushEntityChanges(const std::string& name, const std::vector<EntityChange>& changes);

        std::vector<Hierarchy> SaveHierarchies(const std::vector<Scene::Entity>& roots);

        void RemoveHierarchies(const std::vector<Hierarchy>& hierarchies);

        void RestoreHierarchies(std::vector<Hierarchy> hierarchies);

        Scene::Entity RestoreEntity(const json& j);

        std::vector<Placement> SavePlacements(const std::vector<ECS::Entity>& entities);

        void RestorePlacements(std::vector<Placement> placements);

        void PlaceEntity(Scene::Entity entity, ECS::Entity parent, size_t index);

        json SaveComponents(Scene::Entity entity);

        void RestoreComponents(Scene::Entity entity, const json& j);

        void RemoveComponent(Scene::Entity entity, const std::string& key);

        bool IsSameEntity(const json& a, const json& b);

        // Playback times, physics bodies and shadow cascades change while nothing is edited
        bool IsSameComponent(const std::string& key, const json& a, const json& b);

        json SaveSettings();

        bool IsSameSettings(const json& a, const json& b);

        bool IsSameSetting(const std::string& key, const json& a, const json& b);

        void RestoreSettings(const json& j);

        void ResetStates();

        ECS::Entity Resolve(ECS::Entity entity);

        Ref<Scene::Scene> scene;

        std::unordered_map<ECS::Entity, json> entityStates;
        json settingsState;

        std::vector<Hierarchy> hierarchiesBefore;
        bool changeStarted = false;

        std::vector<Placement> placementsBefore;

        std::vector<EntityChange> editedEntities;

        // Entities which were restored with a new id, because the old one was taken
        std::unordered_map<ECS::Entity, ECS::Entity> remappedEntities;

        std::vector<ECS::Entity> changedEntities;

    };

}
