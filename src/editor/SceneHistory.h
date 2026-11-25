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

        // Changes made by code, e.g. a Lua command of the bridge, become one step. An entity is saved the first time it
        // is about to change, of transforms only the matrices are kept, such that edits of many entities stay cheap
        void BeginScriptChange(const Ref<Scene::Scene>& scene);

        void BeforeEntityChange(Scene::Entity entity);

        void BeforeTransformChange(Scene::Entity entity);

        void EntityCreated(Scene::Entity entity);

        void BeforeEntityDestroy(Scene::Entity entity);

        // For changes which can't say what they touch: the root hierarchy is saved once, before the first of them, and
        // after the command. Changes after it aren't recorded separately
        void BeforeSceneChange();

        // Pushes the step if something changed, returns the number of changed, created and destroyed entities
        size_t EndScriptChange(const std::string& name);

        bool Undo(const Ref<Scene::Scene>& scene);

        bool Redo(const Ref<Scene::Scene>& scene);

        void Clear();

        // The next state is no edit, e.g. after an asset was renamed
        void ResetStates();

        // The components of an entity, to find out what an edit changes
        json GetComponents(const Ref<Scene::Scene>& scene, Scene::Entity entity);

        // Changes the components of other entities like the ones of the source since before: the changed values of
        // components both have, added and removed components. Names and transforms belong to each entity
        void ApplyChanges(const Ref<Scene::Scene>& scene, Scene::Entity source, const json& before,
            const std::vector<Scene::Entity>& entities);

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

        void RestoreComponents(Scene::Entity entity, const json& current, const json& state);

        // Values which differ between before and after are set in the target, others keep their own ones
        static void ApplyDifference(const json& before, const json& after, json& target);

        bool IsSameEntity(const json& a, const json& b);

        // Playback times, physics bodies and shadow cascades change while nothing is edited
        bool IsSameComponent(const std::string& key, const json& a, const json& b);

        json SaveSettings();

        bool IsSameSettings(const json& a, const json& b);

        bool IsSameSetting(const std::string& key, const json& a, const json& b);

        void RestoreSettings(const json& state);

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

        struct ScriptChange {
            bool active = false;
            bool sceneSaved = false;
            std::unordered_map<ECS::Entity, json> entitiesBefore;
            std::unordered_set<ECS::Entity> transformEntities;
            std::vector<std::pair<ECS::Entity, mat4>> transforms;
            std::unordered_set<ECS::Entity> created;
            std::vector<Hierarchy> destroyed;
            std::vector<Hierarchy> sceneBefore;
            // Sky, fog, post processing and the other settings of the scene
            json settingsBefore;
        } scriptChange;

        // Shared by the undo and redo of a step, such that large steps are kept once
        struct ScriptStep {
            std::vector<EntityChange> entities;
            std::vector<ECS::Entity> transformEntities;
            std::vector<mat4> transformsBefore;
            std::vector<mat4> transformsAfter;
            std::vector<Hierarchy> created;
            std::vector<Hierarchy> destroyed;
            std::vector<Hierarchy> sceneBefore;
            std::vector<Hierarchy> sceneAfter;
            json settingsBefore;
            json settingsAfter;
        };

        void ApplyScriptStep(const ScriptStep& step, bool undo);

    };

}
