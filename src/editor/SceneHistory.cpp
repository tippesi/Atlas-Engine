#include "SceneHistory.h"
#include "tools/AssetRenamer.h"

#include "scene/SceneSerializer.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace Atlas::Editor {

    using namespace Scene::Components;

    void SceneHistory::Update(const Ref<Scene::Scene>& scene, const std::unordered_set<ECS::Entity>& selectedEntities) {

        this->scene = scene;

        auto interactionFinished = history.IsInteractionFinished();

        if (interactionFinished && !settingsState.is_null()) {
            auto settings = SaveSettings();
            if (!IsSameSettings(settingsState, settings)) {
                auto before = settingsState;
                history.Push({
                    .name = "Edit scene settings",
                    .undo = [this, before]() { RestoreSettings(before); },
                    .redo = [this, settings]() { RestoreSettings(settings); }
                });
                settingsState = settings;
            }
        }

        if (settingsState.is_null())
            settingsState = SaveSettings();

        // Entities which were deselected in the frame the interaction finished still need to be compared,
        // e.g. a text field is confirmed by clicking on another entity
        std::vector<EntityChange> changes;
        if (interactionFinished) {
            for (auto& [ecsEntity, state] : entityStates) {
                Scene::Entity entity(ecsEntity, &scene->entityManager);
                if (!entity.IsValid())
                    continue;

                // Even without a change the state is updated, such that rounding errors don't add up
                auto current = SaveComponents(entity);
                if (!IsSameEntity(state, current))
                    changes.push_back({ ecsEntity, state, current });
                state = current;
            }
            std::erase_if(entityStates, [&](const auto& state) { return !selectedEntities.contains(state.first); });
        }

        for (auto ecsEntity : selectedEntities) {
            Scene::Entity entity(ecsEntity, &scene->entityManager);
            if (entity.IsValid() && !entityStates.contains(ecsEntity))
                entityStates[ecsEntity] = SaveComponents(entity);
        }

        if (changes.empty())
            return;

        auto nameComponent = changes.size() == 1 ? Scene::Entity(changes.front().entity, &scene->entityManager)
            .TryGetComponent<NameComponent>() : nullptr;
        auto name = changes.size() > 1 ? "Edit " + std::to_string(changes.size()) + " entities" :
            nameComponent ? "Edit " + nameComponent->name : std::string("Edit entity");
        PushEntityChanges(name, changes);

    }

    void SceneHistory::BeginChange(const Ref<Scene::Scene>& scene, const std::vector<Scene::Entity>& roots) {

        this->scene = scene;

        hierarchiesBefore = SaveHierarchies(roots);
        changeStarted = true;

    }

    void SceneHistory::EndChange(const std::string& name, const std::vector<Scene::Entity>& roots) {

        if (!changeStarted)
            return;
        changeStarted = false;

        auto before = std::move(hierarchiesBefore);
        auto after = SaveHierarchies(roots);
        if (before.empty() && after.empty())
            return;

        history.Push({
            .name = name,
            .undo = [this, before, after]() {
                RemoveHierarchies(after);
                RestoreHierarchies(before);
            },
            .redo = [this, before, after]() {
                RemoveHierarchies(before);
                RestoreHierarchies(after);
            }
        });

        // Edits of the changed entities start from their new state
        ResetStates();

    }

    void SceneHistory::BeginMove(const Ref<Scene::Scene>& scene, const std::vector<Scene::Entity>& entities) {

        this->scene = scene;

        std::vector<ECS::Entity> ecsEntities(entities.begin(), entities.end());
        placementsBefore = SavePlacements(ecsEntities);

    }

    void SceneHistory::EndMove(const std::string& name) {

        auto before = std::move(placementsBefore);
        std::vector<ECS::Entity> entities;
        for (const auto& placement : before)
            entities.push_back(placement.entity);
        auto after = SavePlacements(entities);

        auto moved = false;
        for (size_t i = 0; i < before.size(); i++)
            moved |= before[i].parent != after[i].parent || before[i].index != after[i].index;
        if (!moved)
            return;

        history.Push({
            .name = name,
            .undo = [this, before]() { RestorePlacements(before); },
            .redo = [this, after]() { RestorePlacements(after); }
        });

        ResetStates();

    }

    void SceneHistory::BeginEdit(const Ref<Scene::Scene>& scene, const std::vector<Scene::Entity>& entities) {

        this->scene = scene;

        editedEntities.clear();
        for (auto entity : entities)
            if (entity.IsValid())
                editedEntities.push_back({ entity, SaveComponents(entity) });

    }

    void SceneHistory::EndEdit(const std::string& name) {

        std::vector<EntityChange> changes;
        for (auto& change : editedEntities) {
            Scene::Entity entity(change.entity, &scene->entityManager);
            if (!entity.IsValid())
                continue;
            change.after = SaveComponents(entity);
            if (!IsSameEntity(change.before, change.after))
                changes.push_back(change);
        }
        editedEntities.clear();

        if (changes.empty())
            return;

        PushEntityChanges(name, changes);
        ResetStates();

    }

    bool SceneHistory::Undo(const Ref<Scene::Scene>& scene) {

        this->scene = scene;

        changedEntities.clear();
        auto undone = history.Undo();
        ResetStates();
        return undone;

    }

    bool SceneHistory::Redo(const Ref<Scene::Scene>& scene) {

        this->scene = scene;

        changedEntities.clear();
        auto redone = history.Redo();
        ResetStates();
        return redone;

    }

    void SceneHistory::Clear() {

        history.Clear();
        ResetStates();
        remappedEntities.clear();
        changedEntities.clear();

    }

    std::vector<Scene::Entity> SceneHistory::GetChangedEntities() {

        std::vector<Scene::Entity> entities;
        std::unordered_set<ECS::Entity> added;
        for (auto ecsEntity : changedEntities) {
            Scene::Entity entity(Resolve(ecsEntity), &scene->entityManager);
            if (entity.IsValid() && added.insert(entity).second)
                entities.push_back(entity);
        }
        return entities;

    }

    void SceneHistory::PushEntityChanges(const std::string& name, const std::vector<EntityChange>& changes) {

        auto restore = [this, changes](bool undo) {
            for (const auto& change : changes) {
                Scene::Entity entity(Resolve(change.entity), &scene->entityManager);
                if (!entity.IsValid())
                    continue;
                RestoreComponents(entity, SaveComponents(entity), undo ? change.before : change.after);
                changedEntities.push_back(entity);
            }
            };

        history.Push({
            .name = name,
            .undo = [restore]() { restore(true); },
            .redo = [restore]() { restore(false); }
        });

    }

    void SceneHistory::BeginScriptChange(const Ref<Scene::Scene>& scene) {

        this->scene = scene;
        scriptChange = ScriptChange { .active = true, .settingsBefore = SaveSettings() };

    }

    void SceneHistory::BeforeEntityChange(Scene::Entity entity) {

        if (!scriptChange.active || scriptChange.sceneSaved || !entity.IsValid() || scriptChange.entitiesBefore.contains(entity))
            return;
        scriptChange.entitiesBefore[entity] = SaveComponents(entity);

    }

    void SceneHistory::BeforeTransformChange(Scene::Entity entity) {

        if (!scriptChange.active || scriptChange.sceneSaved || !entity.IsValid() || !scriptChange.transformEntities.insert(entity).second)
            return;
        auto transform = entity.TryGetComponent<TransformComponent>();
        if (!transform)
            return;

        scriptChange.transforms.push_back({ entity, transform->matrix });

    }

    void SceneHistory::EntityCreated(Scene::Entity entity) {

        if (scriptChange.active && !scriptChange.sceneSaved)
            scriptChange.created.insert(entity);

    }

    void SceneHistory::BeforeEntityDestroy(Scene::Entity entity) {

        if (!scriptChange.active || scriptChange.sceneSaved || !entity.IsValid())
            return;

        // Entities created by the same command just disappear again
        if (scriptChange.created.erase(entity))
            return;

        // Changes the script made before are taken back, the hierarchy is saved as it was before the command
        auto restoreBefore = [&](auto&& self, Scene::Entity current) -> void {
            if (auto before = scriptChange.entitiesBefore.find(current); before != scriptChange.entitiesBefore.end())
                RestoreComponents(current, SaveComponents(current), before->second);
            for (const auto& [transformEntity, matrix] : scriptChange.transforms)
                if (transformEntity == current && current.HasComponent<TransformComponent>())
                    current.GetComponent<TransformComponent>().Set(matrix);
            if (auto hierarchy = current.TryGetComponent<HierarchyComponent>())
                for (auto child : hierarchy->GetChildren())
                    self(self, child);
            };
        restoreBefore(restoreBefore, entity);
        auto destroyed = SaveHierarchies({ entity });
        scriptChange.destroyed.insert(scriptChange.destroyed.end(), destroyed.begin(), destroyed.end());

    }

    void SceneHistory::BeforeSceneChange() {

        if (!scriptChange.active || scriptChange.sceneSaved)
            return;

        // The root hierarchy as it was before the command: changes recorded so far are undone first, then the saved
        // root replaces the one the script leaves behind
        auto root = scene->GetRootEntity();
        if (!root.IsValid())
            return;
        scriptChange.sceneBefore = SaveHierarchies({ root });
        scriptChange.sceneSaved = true;

    }

    size_t SceneHistory::EndScriptChange(const std::string& name) {

        if (!scriptChange.active)
            return 0;
        auto change = std::move(scriptChange);
        scriptChange = ScriptChange();
        // Selected entities and settings are compared against their state after the script from now on
        ResetStates();

        auto step = CreateRef<ScriptStep>();
        for (auto& [ecsEntity, before] : change.entitiesBefore) {
            Scene::Entity entity(ecsEntity, &scene->entityManager);
            if (!entity.IsValid())
                continue;
            auto after = SaveComponents(entity);
            if (!IsSameEntity(before, after))
                step->entities.push_back({ ecsEntity, std::move(before), std::move(after) });
        }

        // Created entities are restored with their transform, those restored as a whole don't need their matrix
        for (const auto& [ecsEntity, before] : change.transforms) {
            Scene::Entity entity(ecsEntity, &scene->entityManager);
            auto transform = entity.IsValid() ? entity.TryGetComponent<TransformComponent>() : nullptr;
            if (!transform || transform->matrix == before || change.entitiesBefore.contains(ecsEntity) ||
                change.created.contains(ecsEntity))
                continue;
            step->transformEntities.push_back(ecsEntity);
            step->transformsBefore.push_back(before);
            step->transformsAfter.push_back(transform->matrix);
        }

        std::vector<Scene::Entity> createdEntities;
        createdEntities.reserve(change.created.size());
        for (auto ecsEntity : change.created)
            createdEntities.emplace_back(ecsEntity, &scene->entityManager);
        step->created = SaveHierarchies(createdEntities);
        step->destroyed = std::move(change.destroyed);

        if (change.sceneSaved) {
            step->sceneBefore = std::move(change.sceneBefore);
            step->sceneAfter = SaveHierarchies({ scene->GetRootEntity() });
        }

        auto settingsAfter = SaveSettings();
        if (!IsSameSettings(change.settingsBefore, settingsAfter)) {
            step->settingsBefore = std::move(change.settingsBefore);
            step->settingsAfter = std::move(settingsAfter);
        }

        auto count = step->entities.size() + step->transformEntities.size() + step->created.size() + step->destroyed.size() +
            (change.sceneSaved ? size_t(1) : size_t(0)) + (step->settingsAfter.is_null() ? size_t(0) : size_t(1));
        if (count == 0)
            return 0;

        std::shared_ptr<const ScriptStep> data = step;
        history.Push({
            .name = name,
            .undo = [this, data]() { ApplyScriptStep(*data, true); },
            .redo = [this, data]() { ApplyScriptStep(*data, false); }
        });

        return count;

    }

    void SceneHistory::ApplyScriptStep(const ScriptStep& step, bool undo) {

        auto setTransforms = [&](const std::vector<mat4>& matrices) {
            for (size_t i = 0; i < step.transformEntities.size(); i++) {
                Scene::Entity entity(Resolve(step.transformEntities[i]), &scene->entityManager);
                if (!entity.IsValid())
                    continue;
                entity.GetComponent<TransformComponent>().Set(matrices[i]);
                changedEntities.push_back(entity);
            }
            };
        auto setEntities = [&]() {
            for (const auto& entityChange : step.entities) {
                Scene::Entity entity(Resolve(entityChange.entity), &scene->entityManager);
                if (!entity.IsValid())
                    continue;
                RestoreComponents(entity, SaveComponents(entity), undo ? entityChange.before : entityChange.after);
                changedEntities.push_back(entity);
            }
            };

        // The scene was saved after the changes recorded before it, which are undone afterwards and redone before it
        if (undo) {
            if (!step.sceneBefore.empty()) {
                RemoveHierarchies(step.sceneAfter);
                RestoreHierarchies(step.sceneBefore);
            }
            RemoveHierarchies(step.created);
            RestoreHierarchies(step.destroyed);
            setEntities();
            setTransforms(step.transformsBefore);
            if (!step.settingsBefore.is_null())
                RestoreSettings(step.settingsBefore);
        }
        else {
            if (!step.settingsAfter.is_null())
                RestoreSettings(step.settingsAfter);
            setTransforms(step.transformsAfter);
            setEntities();
            RemoveHierarchies(step.destroyed);
            RestoreHierarchies(step.created);
            if (!step.sceneAfter.empty()) {
                RemoveHierarchies(step.sceneBefore);
                RestoreHierarchies(step.sceneAfter);
            }
        }

    }

    std::vector<SceneHistory::Hierarchy> SceneHistory::SaveHierarchies(const std::vector<Scene::Entity>& roots) {

        // Scripts save thousands of roots at once, e.g. all entities they created under the same parent
        std::unordered_set<ECS::Entity> rootSet(roots.begin(), roots.end());
        std::unordered_map<ECS::Entity, std::unordered_map<ECS::Entity, size_t>> childIndices;

        std::vector<Hierarchy> hierarchies;
        for (auto root : roots) {
            if (!root.IsValid())
                continue;

            // Descendants of other roots are part of their hierarchy already
            auto hasRootAncestor = false;
            for (auto parent = scene->GetParentEntity(root); parent.IsValid() && !hasRootAncestor; parent = scene->GetParentEntity(parent))
                hasRootAncestor = rootSet.contains(parent);
            if (hasRootAncestor)
                continue;

            auto& hierarchy = hierarchies.emplace_back();
            auto parent = scene->GetParentEntity(root);
            hierarchy.parent = parent;
            hierarchy.index = 0;
            if (parent.IsValid()) {
                auto [indices, inserted] = childIndices.try_emplace(parent);
                if (inserted) {
                    const auto& children = parent.GetComponent<HierarchyComponent>().GetChildren();
                    for (size_t i = 0; i < children.size(); i++)
                        indices->second[children[i]] = i;
                }
                hierarchy.index = indices->second[root];
            }

            std::set<ECS::Entity> insertedEntities;
            Scene::EntityToJson(hierarchy.entity, root, scene.get(), insertedEntities);
        }
        return hierarchies;

    }

    void SceneHistory::RemoveHierarchies(const std::vector<Hierarchy>& hierarchies) {

        for (const auto& hierarchy : hierarchies) {
            Scene::Entity entity(Resolve(hierarchy.entity["id"]), &scene->entityManager);
            if (entity.IsValid())
                scene->DestroyEntity(entity);
        }

    }

    void SceneHistory::RestoreHierarchies(std::vector<Hierarchy> hierarchies) {

        // Siblings are inserted from the front, such that each one ends up at its index again
        std::sort(hierarchies.begin(), hierarchies.end(),
            [](const Hierarchy& a, const Hierarchy& b) { return a.index < b.index; });

        for (const auto& hierarchy : hierarchies) {
            auto entity = RestoreEntity(hierarchy.entity);
            changedEntities.push_back(entity);

            PlaceEntity(entity, hierarchy.parent, hierarchy.index);
        }

    }

    std::vector<SceneHistory::Placement> SceneHistory::SavePlacements(const std::vector<ECS::Entity>& entities) {

        std::vector<Placement> placements;
        for (auto ecsEntity : entities) {
            Scene::Entity entity(ecsEntity, &scene->entityManager);
            auto parent = scene->GetParentEntity(entity);
            size_t index = 0;
            if (parent.IsValid()) {
                const auto& children = parent.GetComponent<HierarchyComponent>().GetChildren();
                index = size_t(std::find(children.begin(), children.end(), entity) - children.begin());
            }
            placements.push_back({ ecsEntity, parent, index });
        }
        return placements;

    }

    void SceneHistory::RestorePlacements(std::vector<Placement> placements) {

        // Siblings are inserted from the front, such that each one ends up at its index again
        std::sort(placements.begin(), placements.end(),
            [](const Placement& a, const Placement& b) { return a.index < b.index; });

        for (const auto& placement : placements) {
            Scene::Entity entity(Resolve(placement.entity), &scene->entityManager);
            if (!entity.IsValid())
                continue;

            PlaceEntity(entity, placement.parent, placement.index);
            changedEntities.push_back(entity);
        }

    }

    void SceneHistory::PlaceEntity(Scene::Entity entity, ECS::Entity parent, size_t index) {

        Scene::Entity parentEntity(Resolve(parent), &scene->entityManager);
        if (!parentEntity.IsValid())
            return;

        entity.SetParent(parentEntity);
        auto& children = parentEntity.GetComponent<HierarchyComponent>().GetChildren();
        auto iter = std::find(children.begin(), children.end(), entity);
        children.erase(iter);
        children.insert(children.begin() + std::min(index, children.size()), entity);

    }

    Scene::Entity SceneHistory::RestoreEntity(const json& j) {

        auto id = Resolve(j["id"]);
        auto ecsEntity = scene->entityManager.Create(id);
        if (ecsEntity == ECS::EntityConfig::InvalidEntity) {
            ecsEntity = scene->entityManager.Create();
            remappedEntities[id] = ecsEntity;
        }

        Scene::Entity entity(ecsEntity, &scene->entityManager);

        auto components = j;
        components.erase("id");
        components.erase("entities");
        Scene::EntityFromJson(AssetRenamer::UpdatePaths(components), entity, scene.get(), false);

        // Children need their components before the parent adds its hierarchy, see EntityFromJson
        if (j.contains("entities")) {
            std::vector<Scene::Entity> children;
            for (const auto& child : j["entities"])
                children.push_back(RestoreEntity(child));

            auto& hierarchy = entity.AddComponent<HierarchyComponent>();
            hierarchy.root = j["root"];
            for (auto child : children)
                hierarchy.AddChild(child);
        }

        return entity;

    }

    json SceneHistory::GetComponents(const Ref<Scene::Scene>& scene, Scene::Entity entity) {

        this->scene = scene;
        return SaveComponents(entity);

    }

    void SceneHistory::ApplyChanges(const Ref<Scene::Scene>& scene, Scene::Entity source, const json& before,
        const std::vector<Scene::Entity>& entities) {

        this->scene = scene;
        auto after = SaveComponents(source);

        auto belongsToEntity = [](const std::string& key) {
            return key == "root" || key == "name" || key == "transform" || key == "hierarchy";
            };

        std::vector<std::string> changed, removed;
        for (const auto& [key, value] : after.items())
            if (!belongsToEntity(key) && (!before.contains(key) || !IsSameComponent(key, before[key], value)))
                changed.push_back(key);
        for (const auto& [key, value] : before.items())
            if (!belongsToEntity(key) && !after.contains(key))
                removed.push_back(key);

        if (changed.empty() && removed.empty())
            return;

        for (auto entity : entities) {
            auto current = SaveComponents(entity);
            auto components = current;
            for (const auto& key : changed) {
                if (!before.contains(key)) {
                    if (!components.contains(key))
                        components[key] = after[key];
                }
                else if (components.contains(key)) {
                    ApplyDifference(before[key], after[key], components[key]);
                }
            }
            for (const auto& key : removed)
                components.erase(key);
            RestoreComponents(entity, current, components);
        }

    }

    void SceneHistory::ApplyDifference(const json& before, const json& after, json& target) {

        if (before == after)
            return;

        if (before.is_object() && after.is_object() && target.is_object()) {
            for (const auto& [key, value] : after.items()) {
                if (before.contains(key) && target.contains(key))
                    ApplyDifference(before[key], value, target[key]);
                else if (!before.contains(key))
                    target[key] = value;
            }
            for (const auto& [key, value] : before.items())
                if (!after.contains(key))
                    target.erase(key);
            return;
        }

        // Elements of lists with another length don't correspond to each other
        if (before.is_array() && after.is_array() && target.is_array() &&
            before.size() == after.size() && before.size() == target.size()) {
            for (size_t i = 0; i < after.size(); i++)
                ApplyDifference(before[i], after[i], target[i]);
            return;
        }

        target = after;

    }

    json SceneHistory::SaveComponents(Scene::Entity entity) {

        json j;
        Scene::ComponentsToJson(j, entity, scene.get());
        if (auto comp = entity.TryGetComponent<HierarchyComponent>())
            j["root"] = comp->root;

        return j;

    }

    void SceneHistory::RestoreComponents(Scene::Entity entity, const json& current, const json& state) {

        auto j = AssetRenamer::UpdatePaths(state);

        for (const auto& [key, value] : current.items()) {
            if (key == "root" || (j.contains(key) && IsSameComponent(key, j[key], value)))
                continue;

            // The transform is set in place, removing it would detach e.g. the rigid body
            if (key == "transform" && j.contains(key)) {
                TransformComponent transform = j[key];
                auto& comp = entity.GetComponent<TransformComponent>();
                comp.isStatic = transform.isStatic;
                comp.parentJoint = transform.parentJoint;
                comp.Set(transform.matrix);
                continue;
            }

            Scene::RemoveComponent(entity, key);
        }

        json added = json::object();
        for (const auto& [key, value] : j.items()) {
            if (key == "root" || (key == "transform" && current.contains(key)))
                continue;
            if (!current.contains(key) || !IsSameComponent(key, current[key], value))
                added[key] = value;
        }
        Scene::EntityFromJson(added, entity, scene.get(), false);

        auto hierarchy = entity.TryGetComponent<HierarchyComponent>();
        if (hierarchy && j.contains("root"))
            hierarchy->root = j["root"];

    }

    bool SceneHistory::IsSameEntity(const json& a, const json& b) {

        if (a.size() != b.size())
            return false;

        for (const auto& [key, value] : a.items())
            if (!b.contains(key) || !IsSameComponent(key, value, b[key]))
                return false;

        return true;

    }

    bool SceneHistory::IsSameComponent(const std::string& key, const json& a, const json& b) {

        // The local matrix is rebuilt from the global one, which changes it by rounding errors
        if (key == "transform") {
            TransformComponent transformA = a, transformB = b;
            if (transformA.isStatic != transformB.isStatic || transformA.parentJoint != transformB.parentJoint)
                return false;
            for (int32_t i = 0; i < 4; i++) {
                auto tolerance = 1e-6f * glm::max(vec4(1.0f), glm::abs(transformA.matrix[i]));
                if (glm::any(glm::greaterThan(glm::abs(transformA.matrix[i] - transformB.matrix[i]), tolerance)))
                    return false;
            }
            return true;
        }

        if (key != "animation" && key != "audio" && key != "rigidBody" && key != "light" && key != "camera")
            return a == b;

        // Runtime state which isn't edited, e.g. the shadow cascades follow the camera
        auto withoutRuntimeState = [&](json j) {
            if (key == "animation") {
                j.erase("time");
                if (j.contains("layers"))
                    for (auto& layer : j["layers"])
                        layer.erase("time");
            }
            if (key == "audio" && j.contains("stream"))
                j["stream"].erase("time");
            if (key == "rigidBody")
                j.erase("bodyIndex");
            if (key == "light" && j.contains("shadow")) {
                j["shadow"].erase("views");
                j["shadow"].erase("center");
            }
            // Applications give cameras the shape of the viewport unless they keep their aspect ratio
            if (key == "camera" && !j.value("constrainAspectRatio", false))
                j.erase("aspectRatio");
            return j;
            };
        return withoutRuntimeState(a) == withoutRuntimeState(b);

    }

    json SceneHistory::SaveSettings() {

        json j;
        Scene::SceneSettingsToJson(j, scene.get());
        return j;

    }

    bool SceneHistory::IsSameSettings(const json& a, const json& b) {

        if (a.size() != b.size())
            return false;

        for (const auto& [key, value] : a.items())
            if (!b.contains(key) || !IsSameSetting(key, value, b[key]))
                return false;

        return true;

    }

    bool SceneHistory::IsSameSetting(const std::string& key, const json& a, const json& b) {

        if (key != "irradianceVolume")
            return a == b;

        // A scrolling volume follows the camera, only its size is a setting
        auto withoutPosition = [](json j) {
            if (!j.value("scroll", false) || !j.contains("aabb"))
                return j;
            const auto& aabb = j["aabb"];
            auto size = json::array();
            for (const auto* axis : { "x", "y", "z" })
                size.push_back(std::round((aabb["max"][axis].get<float>() - aabb["min"][axis].get<float>()) * 100.0f));
            j.erase("aabb");
            j["size"] = size;
            return j;
            };
        return withoutPosition(a) == withoutPosition(b);

    }

    void SceneHistory::RestoreSettings(const json& state) {

        auto j = AssetRenamer::UpdatePaths(state);

        // Only changed settings are restored, e.g. the irradiance volume would need to be rebuilt
        auto current = SaveSettings();
        json changed = json::object();
        for (const auto& [key, value] : j.items())
            if (!current.contains(key) || !IsSameSetting(key, current[key], value))
                changed[key] = value;

        Scene::SceneSettingsFromJson(changed, scene.get());

        // Features which didn't exist in the restored state are removed again
        if (!j.contains("fog"))
            scene->fog = nullptr;
        if (!j.contains("irradianceVolume"))
            scene->irradianceVolume = nullptr;
        if (!j.contains("reflection"))
            scene->reflection = nullptr;
        if (!j.contains("rtgi"))
            scene->rtgi = nullptr;
        if (!j.contains("sss"))
            scene->sss = nullptr;
        if (!j.contains("ssgi"))
            scene->ssgi = nullptr;
        if (!j.contains("ocean"))
            scene->ocean = nullptr;
        if (!j.contains("navigation"))
            scene->navigation = nullptr;

    }

    void SceneHistory::ResetStates() {

        entityStates.clear();
        settingsState = json();

    }

    ECS::Entity SceneHistory::Resolve(ECS::Entity entity) {

        // Each restore with a new id adds a link, a chain can't be longer than the history
        for (size_t i = 0; i <= history.maxSteps; i++) {
            auto iter = remappedEntities.find(entity);
            if (iter == remappedEntities.end())
                break;
            entity = iter->second;
        }
        return entity;

    }

}
